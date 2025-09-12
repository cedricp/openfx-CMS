/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-misc <https://github.com/NatronGitHub/openfx-misc>,
 * (C) 2018-2021 The Natron Developers
 * (C) 2013-2018 INRIA
 *
 * openfx-misc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openfx-misc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openfx-misc.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

/*
 * OFX ColorBars plugin.
 */

#include "MLVReader.h"
#include <RawLib/idt/dng_idt.h>
#include <RawLib/idt/spectral_idt.h>
#include <RawLib/idt/define.h>
extern "C" {
#include <RawLib/color_aberration/ColorAberrationCorrection.h>
}
#include <cmath>
#include <climits>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <stddef.h>
#include <regex>
#include <filesystem>
#include "ofxOpenGLRender.h"

extern "C"{
#include <dng/dng.h>
}

#define IDT_MUTEX_LOCK _gThreadHost->mutexLock(_idtMutex);
#define IDT_MUTEX_UNLOCK _gThreadHost->mutexUnLock(_idtMutex);

OFXS_NAMESPACE_ANONYMOUS_ENTER

#ifdef __linux__
#include <unistd.h>
#define Sleep(t) usleep(t*1000);
#endif

enum ColorSpaceFormat {
        ACES_AP0,
        ACES_AP1,
        REC709,
        XYZ
};

struct SequenceRange {
    int first;
    int last;
    bool found;
};

SequenceRange detectSequence(const std::string& pattern) {
    const std::string directory = std::filesystem::path(pattern).parent_path().string();
    const std::string basefilename = std::filesystem::path(pattern).filename().string();
    printf("Searching for sequence in directory : %s\n", directory.c_str());
    // Convert pattern "file.####.ext" to regex: "file\.(\d+)\.ext"
    std::string regexPattern = std::regex_replace(basefilename, std::regex("#+"), R"((\d+))");
    std::regex re(regexPattern);

    int minNum = std::numeric_limits<int>::max();
    int maxNum = std::numeric_limits<int>::min();
    bool found = false;

    for (auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();

        std::smatch match;
        if (std::regex_match(filename, match, re)) {
            if (match.size() > 1) {
                int num = std::stoi(match[1].str());
                minNum = std::min(minNum, num);
                maxNum = std::max(maxNum, num);
                found = true;
            }
        }
    }

    if (!found) {
        return {0, 0, false};
    }
    return {minNum, maxNum, true};
}

bool makeFilename(const std::string& pattern, int number, std::string& result) {
    // Find the run of '#' characters
    size_t first = pattern.find('#');
    if (first == std::string::npos) {
        return false;
    }

    size_t last = pattern.find_last_of('#');
    size_t count = last - first + 1;  // number of '#' characters

    // Build number with leading zeros
    std::ostringstream oss;
    oss << std::setw(static_cast<int>(count)) << std::setfill('0') << number;
    std::string numStr = oss.str();

    // Replace the run of '#' with numStr
    result = pattern;
    result.replace(first, count, numStr);

    return true;
}

class ColorProcessor : public OFX::ImageProcessor
{
public:
    ColorProcessor(OFX::ImageEffect &instance): ImageProcessor(instance)
    {

    } 

    ~ColorProcessor()
    {
    }

    void multiThreadProcessImages(const OfxRectI &procWindow, const OfxPointD &rs) OVERRIDE FINAL
    {
        OFX::unused(rs);
        float range = wl - bl;
        scale = 1.0 / range;
        float clipping_value = scale * range * cam_mult.min();
        Vector3f scaledCamMult = cam_mult * scale;

        for (int y = procWindow.y1; y < procWindow.y2; y++)
        {
            if (y >= raw_height) break;
            float *dstPix = static_cast<float*>(_dstImg->getPixelAddress(procWindow.x1, y) );
            uint16_t* srcPix = raw_buffer + (raw_height - 1 - y) * (raw_width * 3) + (procWindow.x1 * 3);
            for (int x = procWindow.x1; x < procWindow.x2; ++x)
            {
                if (x > raw_width) break;
                Vector3f in((float)srcPix[0], (float)srcPix[1], (float)srcPix[2]);
                in *= scaledCamMult;
                if (clip){
                    in.clip_in_place(0.f, clipping_value);
                }

                Vector3f out = idt_matrix * in;
                out *= headroom; // Apply headroom
                out.copy_to(dstPix);

                dstPix[3] = 1.f;
                dstPix += 4;
                srcPix += 3;
            }
        }
    }

    Matrix3x3f idt_matrix;
    OFX::Image *srcImg;
    float scale, wl, bl;
    uint16_t *raw_buffer;
    Vector3f cam_mult;
    int raw_width, raw_height;
    bool clip;
    float headroom = 1.0f; // Headroom value for the output
};

bool MLVReaderPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!kSupportsRenderScale && ((args.renderScale.x != 1.) || (args.renderScale.y != 1.))) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    if (_dng_sequence.size() > 0){
        if (_dngh == 0 || _dngw == 0){
            return false;
        }
        rod.x1 = 0;
        rod.x2 = _dngw;
        rod.y1 = 0;
        rod.y2 = _dngh;
        return true;
    }

    if (_mlv_video.empty()){
        return false;
    }

    rod.x1 = 0;
    rod.x2 = _mlv_video[0]->raw_resolution_x();
    rod.y1 = 0;
    rod.y2 = _mlv_video[0]->raw_resolution_y();
    return true;
}

bool MLVReaderPlugin::prepareSprectralSensIDT(Dng_processor* dng)
{
    // matMethod0
    // No color space conversion
    // No camera matrix
    std::string datapath = _pluginPath + "/Contents/Resources/data";
    
    std::string camera_make;
    std::string camera_model;

    if (_mlv_video.size() > 0){
        camera_make = _mlv_video[0]->get_camera_make();
        camera_model = _mlv_video[0]->get_camera_model();
    } else if (dng){
        camera_make = dng->camera_make();
        camera_model = dng->camera_model();
    } else {
        return false;
    }

    _useSpectralIdt->setEnabled(false);
    
    SSIDT::Idt idt;
    int ok = 0;
    ok = idt.loadIlluminant( vector<string>(), "na" );
    if (!ok)
    {
        return false;
    }
    
    ok = 0;
    std::vector< std::string > jsonfiles = openDir(datapath + "/camera");
    for (auto json : jsonfiles){
        ok = idt.loadCameraSpst(json, camera_make.c_str(), camera_model.c_str());
        if (ok) break;
    }

    if (!ok)
    {
        return false;
    }

    _useSpectralIdt->setEnabled(true);

    if (!_useSpectralIdt->getValue()){
        // Don't use this IDT, but we know it's possible
        return false;
    }

    idt.loadTrainingData(datapath + "/training/training_spectral.json");
    
    // Set the user white balance coeffs
    std::vector< double > dcoeffs (_asShotNeutral.data(), _asShotNeutral.data() + 3);
    idt.chooseIllumSrc( dcoeffs, 0 );

    if ( idt.calIDT() )
    {
        idt.getIdtF(_idt.data());
        idt.getWBF(_asShotNeutral.data());
        return true;
    }

    _useSpectralIdt->setValue(false);

    return false;
}

void MLVReaderPlugin::computeIDT(Dng_processor* dng)
{
    int colorspace = _outputColorSpace->getValue();
    
    if (colorspace == 3){
        return;
    }

    IDT_MUTEX_LOCK

    if (dng){
        dng->get_white_balance_coeffs(_colorTemperature->getValue(), _asShotNeutral.data(), _cameraWhiteBalance->getValue());
        if (!prepareSprectralSensIDT()){
            DNGIdt::DNGIdt idt(dng, _colorTemperature->getValue(), _cameraWhiteBalance->getValue());
            idt.getDNGIDTMatrix(_idt.data(), colorspace);

            // Clear checkbox
            _useSpectralIdt->setValue(false);
        }
    }
    else if (_mlv_video.size() > 0)
    {
        // Thread safe...
        _mlv_video[0]->get_white_balance_coeffs(_colorTemperature->getValue(), _asShotNeutral.data(), _cameraWhiteBalance->getValue());
        if (!prepareSprectralSensIDT()){
            // No spectral sensitivities IDT, fall back to DNG IDT
            DNGIdt::DNGIdt idt(_mlv_video[0], _colorTemperature->getValue(), _cameraWhiteBalance->getValue());
            idt.getDNGIDTMatrix(_idt.data(), colorspace);

            // Clear checkbox
            _useSpectralIdt->setValue(false);
        }
    }

    IDT_MUTEX_UNLOCK
}

Mlv_video* MLVReaderPlugin::getMlv()
{
    Mlv_video *mlv_video = nullptr;

    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return nullptr;
    for (unsigned int i = 0; i < _mlv_video.size(); ++i){
        if (!_mlv_video[i]->locked()){
            _mlv_video[i]->lock();
            mlv_video = _mlv_video[i];
            break;
        }
    }
    _gThreadHost->mutexUnLock(_videoMutex);

    return mlv_video;
}

Dng_processor* MLVReaderPlugin::getDng(double time)
{
    Dng_processor *dngproc = nullptr;

    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return nullptr;
    
    for (unsigned int i = 0; i < _dng_sequence.size(); ++i){
        if (!_dng_sequence[i]->locked()){
            _dng_sequence[i]->lock();
            dngproc = _dng_sequence[i];
            std::string file = _mlvfilename_param->getValue();
            makeFilename(file, (int)time, file);
            dngproc->load_dng(file);
            break;
        }
    }

    _gThreadHost->mutexUnLock(_videoMutex);

    return dngproc;
}

// the overridden render function
void MLVReaderPlugin::render(const OFX::RenderArguments &args)
{
    Mlv_video *mlv_video = getMlv();
    Dng_processor * dng_img = getDng(args.time);

    if (mlv_video == nullptr && dng_img == nullptr){
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }

    if (_idtDirty){
        computeIDT(dng_img);
        _idtDirty = false;
    }

    const int time = floor(args.time+0.5);

    if (FOCUSPIXELMAP_OK == 0 && _fixFocusPixel->getValue()){
        setPersistentMessage(OFX::Message::eMessageWarning, "", std::string("Warning : Focus pixel map not found"));
    }
    
    OFX::auto_ptr<OFX::Image> dst(_outputClip->fetchImage(args.time));
    
    if (!dst)
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    
    int raw_width = 0;
    int raw_height = 0;

    if (mlv_video){
        raw_width = mlv_video->raw_resolution_x();
        raw_height = mlv_video->raw_resolution_y();
    } else {
        raw_width = dng_img->width();
        raw_height = dng_img->height();
    }
    
    OfxRectI renderWin = args.renderWindow;
    int width_img = (int)(renderWin.x2 - renderWin.x1);
    int height_img = (int)(renderWin.y2 - renderWin.y1);
    
    if (_debayerType->getValue() == 0){
        int dng_size = 0;
        float max_value = _maxValue;
        uint16_t* raw_buffer = nullptr;
        if (mlv_video != nullptr){
            // Extract raw buffer - No processing (debug)
            Mlv_video::RawInfo  info;
            mlv_video->low_level_process(info);
            mlv_video->get_dng_buffer(time, dng_size, true);
            raw_buffer = mlv_video->postprocecessed_raw_buffer();
        } else {
            float max_value = _maxValue;
            raw_buffer = dng_img->get_processed_filebuffer();
        }

        if (raw_buffer != nullptr){
            for(int y=renderWin.y1; y < renderWin.y2; y++) {
                uint16_t* srcPix = raw_buffer + ((width_img) * (height_img - 1 - y)) + ((int)renderWin.x1 * 4);
                float *dstPix = (float*)dst->getPixelAddress((int)renderWin.x1, y+(int)renderWin.y1);
                for(int x=renderWin.x1; x < renderWin.x2; x++) {
                    float pixel_val = float(*srcPix++) / max_value;
                    *dstPix++ = pixel_val;
                    *dstPix++ = pixel_val;
                    *dstPix++ = pixel_val;
                    *dstPix++ = 1.f;
                }
            }
        }
    } else {
        if (dng_img)
        {
            //if (getUseOpenCL()){
            //    renderDNG_GPU(args, dst.get(), dng_img, time, height_img, width_img);
            //} else {
            //    renderDNG_CPU(args, dst.get(), dng_img, time, height_img, width_img);
            //}
            renderDNG_CPU(args, dst.get(), dng_img, time, height_img, width_img);
        }
        else
        {
            bool darkframe_fileok = std::filesystem::exists(_mlv_darkframefilename->getValue());
            _enableDarkFrame->setEnabled(darkframe_fileok);
            if (!darkframe_fileok){
                darkframe_fileok = false;
                if (_enableDarkFrame->getValue() == true){
                    setPersistentMessage(OFX::Message::eMessageWarning, "", std::string("Warning : Dark frame file not found, dark frame correction will not be applied"));
                }
            }

            // Common code for CPU and OpenCL
            Mlv_video::RawInfo rawInfo;
            rawInfo.dual_iso_mode = _dualIsoMode->getValue();
            rawInfo.chroma_smooth = _chromaSmooth->getValue();
            rawInfo.fix_focuspixels = _fixFocusPixel->getValue();
            rawInfo.dualisointerpolation = _dualIsoAveragingMethod->getValue(); 
            rawInfo.dualiso_fullres_blending = _dualIsoFullresBlending->getValue();
            rawInfo.dualiso_aliasmap = _dualIsoAliasMap->getValue();
            rawInfo.darkframe_file = _mlv_darkframefilename->getValue();
            rawInfo.darkframe_enable = darkframe_fileok;
            mlv_video->low_level_process(rawInfo);

            if (getUseOpenCL()){
                renderMLV_GPU(args, dst.get(), mlv_video, time);
                
            } else {
                renderMLV_CPU(args, dst.get(), mlv_video, time, raw_height, raw_width);
            }

        }

        float cacorrection_threshold = _cacorrection_threshold->getValue();
        if(cacorrection_threshold < 1.0){
            uint8_t cacorrection_radius = (uint8_t)_cacorrection_radius->getValue();
            // Apply color aberration correction
            CACorrection(width_img, height_img, (float*)dst.get()->getPixelData(), cacorrection_threshold, cacorrection_radius);
        }
    }

    if (mlv_video) mlv_video->unlock();
    if (dng_img) dng_img->unlock();    
}

void MLVReaderPlugin::renderCL(const OFX::RenderArguments &args, OFX::Image* dst, uint16_t* raw_buffer, int black_level, int white_level, int time, int height, int width)
{
    Matrix3x3f cam_matrix;
    computeColorspaceMatrix(cam_matrix);

    // Compute clip values
    float scale = 1. / float(white_level - black_level);
    float clipping_value = scale * float(white_level - black_level) * _asShotNeutral.min();

    if (_highlightMode->getValue() > 0){
        clipping_value = 10000.f;
    }

    cl::Image2D img_in(getCurrentCLContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_R, CL_UNSIGNED_INT16), width, height, 0, raw_buffer);
    cl::Image2D img_out(getCurrentCLContext(), CL_MEM_WRITE_ONLY, cl::ImageFormat(CL_RGBA, CL_FLOAT), width, height, 0, NULL);
    cl::Image2D img_tmp(getCurrentCLContext(), CL_MEM_READ_WRITE, cl::ImageFormat(CL_RGBA, CL_FLOAT), width, height, 0, NULL);
    
    cl::Event timer;
    cl::CommandQueue queue = cl::CommandQueue(getCurrentCLContext(), getCurrentCLDevice());

    // Standard Canon filter (RGGB)
    uint32_t bayer_filter = 0x94949494;
    {
        // Process borders
        OpenCLLocalBufferStruct locopt
        = (OpenCLLocalBufferStruct){ .xoffset = 2*1, .xfactor = 1, .yoffset = 2*1, .yfactor = 1,
                                     .cellsize = 4 * sizeof(float), .overhead = 0,
                                     .sizex = 1 << 8, .sizey = 1 << 8 };
        cl::Kernel kernel_demosaic_border(getProgram("debayer_ppg"), "border_interpolate");
        if (!openCLGetLocalBufferOpt(getCurrentCLDevice(), kernel_demosaic_border, &locopt)){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Invalid work dimension (border_interpolate)"));
            return;
        }

        kernel_demosaic_border.setArg(0, img_in);
        kernel_demosaic_border.setArg(1, img_tmp);
        kernel_demosaic_border.setArg(2, width);
        kernel_demosaic_border.setArg(3, height);
        kernel_demosaic_border.setArg(4, bayer_filter);
        kernel_demosaic_border.setArg(5, 3);
        kernel_demosaic_border.setArg(6, black_level);
        kernel_demosaic_border.setArg(7, white_level);
        kernel_demosaic_border.setArg(8, _asShotNeutral[0]);
        kernel_demosaic_border.setArg(9, _asShotNeutral[2]);
        kernel_demosaic_border.setArg(10, clipping_value);

        cl::NDRange sizes(width, height);
        
        bool ok = queue.enqueueNDRangeKernel(kernel_demosaic_border, cl::NullRange, sizes, cl::NullRange, NULL, &timer);
        if (ok != CL_SUCCESS){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Border interpolation failed"));
            return;
        }
    }

    {
        // Process green channel
        OpenCLLocalBufferStruct locopt
        = (OpenCLLocalBufferStruct){ .xoffset = 2*3, .xfactor = 1, .yoffset = 2*3, .yfactor = 1,
                                     .cellsize = sizeof(float) * 1, .overhead = 0,
                                     .sizex = 1 << 8, .sizey = 1 << 8 };
        cl::Kernel kernel_demosaic_green(getProgram("debayer_ppg"), "ppg_demosaic_green");
        if (!openCLGetLocalBufferOpt(getCurrentCLDevice(), kernel_demosaic_green, &locopt)){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Invalid work dimension (green)"));
            return;
        }
        
        cl::NDRange sizes( ROUNDUP(width, locopt.sizex), ROUNDUP(height, locopt.sizey) );
        cl::NDRange local( locopt.sizex, locopt.sizey );

        kernel_demosaic_green.setArg(0, img_in);
        kernel_demosaic_green.setArg(1, img_tmp);
        kernel_demosaic_green.setArg(2, width);
        kernel_demosaic_green.setArg(3, height);
        kernel_demosaic_green.setArg(4, bayer_filter);
        kernel_demosaic_green.setArg(5, black_level);
        kernel_demosaic_green.setArg(6, white_level);
        kernel_demosaic_green.setArg(7, sizeof(float) * (locopt.sizex + 2*3) * (locopt.sizey + 2*3), nullptr);
        kernel_demosaic_green.setArg(8, _asShotNeutral[0]);
        kernel_demosaic_green.setArg(9, _asShotNeutral[2]);
        kernel_demosaic_green.setArg(10, clipping_value);
        
        bool ok = queue.enqueueNDRangeKernel(kernel_demosaic_green, cl::NullRange, sizes, local, NULL, &timer);
        if (ok != CL_SUCCESS){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Green channel demosaic failed"));
            return;
        }
    }

    {
        // Process red/blue channels
        OpenCLLocalBufferStruct locopt
        = (OpenCLLocalBufferStruct){  .xoffset = 2*1, .xfactor = 1, .yoffset = 2*1, .yfactor = 1,
                                    .cellsize = 4 * sizeof(float), .overhead = 0,
                                    .sizex = 1 << 8, .sizey = 1 << 8 };
        cl::Kernel kernel_demosaic_redblue(getProgram("debayer_ppg"), "ppg_demosaic_redblue");
        if (!openCLGetLocalBufferOpt(getCurrentCLDevice(), kernel_demosaic_redblue, &locopt)){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Invalid work dimension (red/blue)"));
            return;
        }

        float headroom = _outputColorSpace->getValue() < 2 ? _headroom->getValue() : 1;

        
        cl::NDRange sizes( ROUNDUP(width, locopt.sizex), ROUNDUP(height, locopt.sizey) );
        cl::NDRange local( locopt.sizex, locopt.sizey );
        
        // matrixbufer [0-8] = forward matrix, [9-17] = idt matrix
        cl::Buffer matrixbuffer(getCurrentCLContext(), CL_MEM_READ_ONLY, sizeof(float) * 9);
        kernel_demosaic_redblue.setArg(0, img_tmp);
        kernel_demosaic_redblue.setArg(1, img_out);
        kernel_demosaic_redblue.setArg(2, width);
        kernel_demosaic_redblue.setArg(3, height);
        kernel_demosaic_redblue.setArg(4, bayer_filter);
        kernel_demosaic_redblue.setArg(5, matrixbuffer);
        kernel_demosaic_redblue.setArg(6, headroom);
        kernel_demosaic_redblue.setArg(7, sizeof(float) * 4 * (locopt.sizex + 2) * (locopt.sizey + 2), nullptr);
        
        queue.enqueueWriteBuffer(matrixbuffer, CL_TRUE, 0, sizeof(float) * 9, cam_matrix.data());
        bool ok = queue.enqueueNDRangeKernel(kernel_demosaic_redblue, cl::NullRange, sizes, local, NULL, &timer);
        if (ok != CL_SUCCESS){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Green channel demosaic failed"));
            return;
        }
    }

    // Fetch result from GPU
    cl::array<size_t, 3> origin = {(size_t)args.renderWindow.x1, (size_t)args.renderWindow.y1, 0};
    cl::array<size_t, 3> size = {(size_t)(args.renderWindow.x2 - args.renderWindow.x1), (size_t)(args.renderWindow.y2 - args.renderWindow.y1), 1};
    queue.enqueueReadImage(img_out, CL_TRUE, origin, size, 0, 0, (float*)dst->getPixelData());
    queue.finish();
}

void MLVReaderPlugin::renderMLV_CPU(const OFX::RenderArguments &args, OFX::Image* dst, Mlv_video* mlv_video, int time, int height_img, int width_img)
{
    int dng_size = 0;
    mlv_video->set_dng_raw_levels(_blackLevel->getValue(), _whiteLevel->getValue());
    uint16_t* dng_buffer = mlv_video->get_dng_buffer(time, dng_size, false);

    if (_levelsDirty){
        _blackLevel->setValue(mlv_video->black_level());
        _whiteLevel->setValue(mlv_video->white_level());
        _resetLevels->setValue(false);
        _levelsDirty = false;
    }
    
    if (dng_buffer == nullptr || dng_size == 0){
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }

    // Note : Libraw needs to be compiled with multithreading (reentrant) support and no OpenMP support
    Dng_processor dng_processor;

    int highlight_mode = _highlightMode->getValue();
    dng_processor.set_interpolation(_debayerType->getValue()-1);
    dng_processor.set_highlight(highlight_mode);

    // Get raw buffer -> raw colors
    uint16_t* processed_buffer = dng_processor.get_processed_image((uint8_t*)dng_buffer, dng_size);
    free(dng_buffer);
    
    ColorProcessor processor(*this);
    processor.setDstImg(dst);
    processor.raw_buffer = processed_buffer;
    processor.setRenderWindow(args.renderWindow, args.renderScale);
    processor.wl =_whiteLevel->getValue();
    processor.bl = _blackLevel->getValue();
    processor.raw_width = width_img;
    processor.raw_height = height_img;
    processor.cam_mult = _asShotNeutral;
    processor.clip = _highlightMode->getValue() == 0;
    processor.headroom = _outputColorSpace->getValue() < 2 ? _headroom->getValue() : 1;
    computeColorspaceMatrix(processor.idt_matrix);

    processor.process();
}

void MLVReaderPlugin::renderDNG_CPU(const OFX::RenderArguments &args, OFX::Image* dst, Dng_processor* dng, int time, int height_img, int width_img)
{
    // Get raw buffer -> raw colors
    uint16_t* processed_buffer = dng->get_processed_filebuffer();

    if (processed_buffer == nullptr) {      
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }

    if (_levelsDirty){
        _blackLevel->setValue(dng->black_level());
        _whiteLevel->setValue(dng->white_level());
        _resetLevels->setValue(false);
        _levelsDirty = false;
    }
    
    ColorProcessor processor(*this);
    processor.setDstImg(dst);
    processor.raw_buffer = processed_buffer;
    processor.setRenderWindow(args.renderWindow, args.renderScale);
    processor.wl =_whiteLevel->getValue();
    processor.bl = _blackLevel->getValue();
    processor.raw_width = width_img;
    processor.raw_height = height_img;
    processor.cam_mult = _asShotNeutral;
    processor.clip = _highlightMode->getValue() == 0;
    processor.headroom = _outputColorSpace->getValue() < 2 ? _headroom->getValue() : 1;
    computeColorspaceMatrix(processor.idt_matrix);

    processor.process();
}

void MLVReaderPlugin::renderDNG_GPU(const OFX::RenderArguments &args, OFX::Image* dst, Dng_processor* dng, int time, int height_img, int width_img)
{
    // Get raw buffer -> raw colors
    uint16_t* processed_buffer = dng->get_processed_filebuffer();

    if (_levelsDirty){
        _blackLevel->setValue(dng->black_level());
        _whiteLevel->setValue(dng->white_level());
        _resetLevels->setValue(false);
        _levelsDirty = false;
    }

    renderCL(args, dst, processed_buffer, _blackLevel->getValue(), _whiteLevel->getValue(), time, height_img, width_img);
}

void MLVReaderPlugin::renderMLV_GPU(const OFX::RenderArguments &args, OFX::Image* dst, Mlv_video* mlv_video, int time)
{
    int dng_size = 0;

    mlv_video->get_dng_buffer(time, dng_size, true);
    uint16_t* raw_buffer = mlv_video->postprocecessed_raw_buffer();

    if (_levelsDirty){
        _blackLevel->setValue(mlv_video->black_level());
        _whiteLevel->setValue(mlv_video->white_level());
        _resetLevels->setValue(false);
        _levelsDirty = false;
    }

    
    uint32_t black_level = _blackLevel->getValue();  
    uint32_t white_level = _whiteLevel->getValue();
    
    int width = mlv_video->raw_resolution_x();
    int height = mlv_video->raw_resolution_y();

    renderCL(args, dst, raw_buffer, black_level, white_level, time, height, width);
}

void MLVReaderPlugin::computeColorspaceMatrix(Matrix3x3f& out_matrix, Dng_processor* dng)
{
    if (dng)
    {
        if (_useSpectralIdt->getValue()){
            // Spectral sensitivity based matrix
            out_matrix = _idt;
        } else {
            Matrix3x3f xyzd65tocam, rgb2rgb;
            int colorspace = _outputColorSpace->getValue();

            xyzd65tocam = dng->matrix2();
            
            rgb2rgb = get_neutral_cam2rec709_matrix(xyzd65tocam);
            if (colorspace <= REC709){
                // Using DNG IDT matrix
                out_matrix = _idt * rec709_to_xyzD50_matrix<float>() * rgb2rgb;
            } else {
                // XYZD50 output
                out_matrix = rec709_to_xyzD50_matrix<float>() * rgb2rgb;
            }
        }
        dng->unlock();
    } else {
        Mlv_video * mlv_video = getMlv();
        if (mlv_video == nullptr){
            return;
        }

        if (_useSpectralIdt->getValue()){
            // Spectral sensitivity based matrix
            out_matrix = _idt;
        } else {
            Matrix3x3f xyzd65tocam, rgb2rgb;
            int colorspace = _outputColorSpace->getValue();

            mlv_video->get_camera_matrix2f(xyzd65tocam.data());
            
            rgb2rgb = get_neutral_cam2rec709_matrix(xyzd65tocam);
            if (colorspace <= REC709){
                // Using DNG IDT matrix
                out_matrix = _idt * rec709_to_xyzD50_matrix<float>() * rgb2rgb;
            } else {
                // XYZD50 output
                out_matrix = rec709_to_xyzD50_matrix<float>() * rgb2rgb;
            }
        }

        mlv_video->unlock();
    }
}

bool MLVReaderPlugin::getTimeDomain(OfxRangeD& range)
{
    Mlv_video* mlv = getMlv();
    if (mlv == nullptr) return false;

    range.min = 1;
    range.max = mlv->frame_count();
    mlv->unlock();

    return true;
}

bool MLVReaderPlugin::isIdentity(const OFX::IsIdentityArguments& /*args*/, OFX::Clip*& /*identityClip*/, double& /*identityTime*/, int& /*view*/, std::string& /*plane*/)
{
    return false;
}

void MLVReaderPlugin::setMlvFile(std::string file, bool set)
{
    std::string extension = file.substr(file.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::toupper);

    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return;

    for(auto dng: _dng_sequence){
        while (dng->locked()){Sleep(10);}
        delete dng;
    }

    for(auto mlv: _mlv_video){
        while (mlv->locked()){Sleep(10);}
        delete mlv;
    }

    _dng_sequence.clear();
    _mlv_video.clear();

    if (extension == "DNG"){
        SequenceRange seq = detectSequence(file);
        if (seq.found){
            OfxPointI tr;
            tr.x = seq.first;
            tr.y = seq.last;
            _timeRange->setValue(tr);
            makeFilename(file, seq.first, file);
        } else {
            OfxPointI tr;
            tr.x = 0;
            tr.y = 0;
            _timeRange->setValue(tr);
        }

        Dng_processor* _dng_img = new Dng_processor;
        _dng_img->lock();
        if (_dng_img->load_dng(file.c_str())){
            _dngh = _dng_img->height();
            _dngw = _dng_img->width();
            _maxValue = pow(2, _dng_img->bpp());
            _bpp->setEnabled(true);
            _bpp->setValue(_dng_img->bpp());
            _bpp->setEnabled(false);

            _dng_sequence.push_back(_dng_img);
            _dng_img->unlock();
            for (int i = 0; i < _numThreads+4; ++i){
                Dng_processor* dng = new Dng_processor;
                _dng_sequence.push_back(dng);
            }
        } else {
            delete _dng_img;
        }
    } else {
        // As mlv-lib does not support multi threading
        // because of file operations, I just create
        // multiples instances
        Mlv_video* mlv_video = new Mlv_video(file);

        if (!mlv_video->valid()){
            delete mlv_video;
        } else {
            _maxValue = pow(2, mlv_video->bpp());
            OfxPointI tr;
            tr.x = 0;
            tr.y = mlv_video->frame_count();
            _timeRange->setValue(tr);
            _mlv_fps->setValue(mlv_video->fps());
            _mlv_video.push_back(mlv_video);
            _bpp->setEnabled(true);
            _bpp->setValue(mlv_video->bpp());
            _bpp->setEnabled(false);
            _levelsDirty = false;
            if (set){
                _levelsDirty = true;
            }
            // Reserve as MLV readers as threads + some extra for safety 
            for (unsigned int i = 0; i < _numThreads+4; ++i){
                // Copy video stream, fast way
                Mlv_video* mlv_videodup = new Mlv_video(*mlv_video);
                _mlv_video.push_back(mlv_videodup);
            }
    
            _idtDirty = true;
        }
    }

    _gThreadHost->mutexUnLock(_videoMutex);
}

void MLVReaderPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    OfxRectI format;
    float fps = 25.f;

    if (_dng_sequence.size() > 0)
    {
        if (_dng_sequence.size())
        {
            format.x1 = 0;
            format.x2 = _dngw;
            format.y1 = 0;
            format.y2 = _dngh;
        }
    }
    else if (_mlv_video.size())
    {
        format.x1 = 0;
        format.x2 = _mlv_video[0]->raw_resolution_x();
        format.y1 = 0;
        format.y2 = _mlv_video[0]->raw_resolution_y();
        fps = _mlv_video[0]->fps();
    }
    else
    {
        return;
    }
    
    // MLV clip is a video stream
    clipPreferences.setOutputFrameVarying(true);
    clipPreferences.setOutputFormat(format);
    
    clipPreferences.setPixelAspectRatio(*_outputClip, 1);
    clipPreferences.setClipBitDepth(*_outputClip, OFX::eBitDepthFloat);
    clipPreferences.setClipComponents(*_outputClip, OFX::ePixelComponentRGBA);
    clipPreferences.setOutputFrameRate(fps);
    clipPreferences.setOutputPremultiplication(OFX::eImageUnPreMultiplied);
    clipPreferences.setOutputHasContinuousSamples(false);
}

void MLVReaderPlugin::changedClip(const OFX::InstanceChangedArgs& /*p_Args*/, const std::string& p_ClipName)
{
    if (p_ClipName == kOfxImageEffectSimpleSourceClipName)
    {
        
    }
}

void MLVReaderPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    desc.setLabel(kPluginName);
    desc.setPluginDescription(kPluginDescription);
    desc.setPluginGrouping(kPluginGrouping);
    desc.addSupportedContext(OFX::eContextGeneral);
    #ifdef OFX_EXTENSIONS_TUTTLE
    desc.addSupportedContext(OFX::eContextReader);
    #endif
    desc.addSupportedContext(OFX::eContextGenerator);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(true);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderTwiceAlways(false);
    desc.setRenderThreadSafety(OFX::kRenderThreadSafety);
    desc.setUsesMultiThreading(true);
}

void MLVReaderPlugin::changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName)
{
    if (paramName == kMLVfileParamter)
    {
        if (args.reason != OFX::eChangeTime) {
            std::string filename = _mlvfilename_param->getValue();
            std::string fileattime;
            _mlvfilename_param->getValueAtTime(args.time, fileattime);
            printf("Loading file : %s\n", fileattime.c_str());
            std::string upperfn = filename;
            std::transform(upperfn.begin(), upperfn.end(), upperfn.begin(), ::toupper);
            bool is_mlv = upperfn.find(".MLV");
            bool is_dng = upperfn.find(".DNG");
            bool is_cr2 = upperfn.find(".CR2");
            if (!is_mlv || !is_dng || !is_cr2){
                setPersistentMessage(OFX::Message::eMessageError, "", std::string("Unsupported file extension"));
                OFX::throwSuiteStatusException(kOfxStatFailed);
                return;
            }

            if (filename != _mlvfilename){
                _mlvfilename = filename;
                setMlvFile(filename);
            }
        }
    }

    if (paramName == kCameraWhiteBalance){
        _colorTemperature->setEnabled(_cameraWhiteBalance->getValue() == false);
        _idtDirty = true;
    }

    if (paramName == kColorSpaceFormat || paramName == kColorTemperature)
    {
        _idtDirty = true;
        _headroom->setEnabled(_outputColorSpace->getValue() < 2);
    }

    if (paramName == kUseSpectralIdt)
    {
        if (_useSpectralIdt->getValue()){
            _outputColorSpace->setValue(0);
            _outputColorSpace->setEnabled(false);
        } else {
            _outputColorSpace->setEnabled(true);
        }
        _idtDirty = true;
    }

    if (paramName == kAudioExport){
        std::string filename = _mlv_audiofilename->getValue();
        if (filename.empty()) return;
        if (_mlv_video.empty()) return;
        
        Mlv_video  *mlv_video = getMlv();

        if (mlv_video){
            mlv_video->write_audio(filename);
            mlv_video->unlock();
        } 
    }

    if (paramName == kDarkFrameButton){
        int sf = _darkframeRange->getValue().x;
        int ef = _darkframeRange->getValue().y;
        if (sf >= ef) return;
        std::string filename = _mlv_darkframefilename->getValue();
        if (filename.empty()) return;
        if (_mlv_video.empty()) return;
        
        Mlv_video  *mlv_video = getMlv();

        if (mlv_video)
        {
            mlv_video->generate_darkframe(filename.c_str(), sf, ef);
            mlv_video->unlock();
        }

        for (auto &mlv : _mlv_video){
            // Wait for the videostream to be released by renderer
            while (mlv->locked()) {Sleep(100);}
            // Destroy darkframe data on all streams
            mlv->destroy_darkframe_data();
        }
        // This is the only way I found to invalidate the playback cache
        _enableDarkFrame->setValue(false);
        _enableDarkFrame->setValue(true);
    }

    if (paramName == kDualIso){
        _levelsDirty = true;
        bool enabled = _dualIsoMode->getValue() > 0;
        _dualIsoAliasMap->setEnabled(enabled);
        _dualIsoAveragingMethod->setEnabled(enabled);
        _dualIsoFullresBlending->setEnabled(enabled);
    }

    if (paramName == kResetLevels){
        _levelsDirty = true;
    }

    if (OpenCLBase::changedParamCL(this, args, paramName))
    {
        _debayerType->setEnabled(getUseOpenCL() == false);
    }
}

void MLVReaderPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
    OFX::ContextEnum context)
{
    // There has to be an input clip
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);

    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setOptional(true);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    dstClip->setSupportsTiles(kSupportsTiles);

    // Create pages
    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");
    OFX::PageParamDescriptor *page_raw = desc.definePageParam("Raw processing");
    OFX::PageParamDescriptor *page_debayer = desc.definePageParam("Debayering");
    OFX::PageParamDescriptor *page_colors = desc.definePageParam("Colors");
    OFX::PageParamDescriptor *page_dualiso = desc.definePageParam("Dual iso");
    OFX::PageParamDescriptor *page_audio = desc.definePageParam("Audio");

    // Create parameters
    {
        OFX::Int2DParamDescriptor *param = desc.defineInt2DParam(kFrameRange);
        //desc.addClipPreferencesSlaveParam(*param);
        param->setLabel("Frame range");
        param->setHint("The video frame range");
        param->setDefault(0, 0);
        param->setEnabled(false);
        if (page)
        {
            page->addChild(*param);
        }
    }

    {
        OFX::DoubleParamDescriptor *param = desc.defineDoubleParam(kMlvFps);
        //desc.addClipPreferencesSlaveParam(*param);
        param->setLabel("FPS");
        param->setHint("The video frame rate in frame per seconds");
        param->setDefault(0);
        param->setEnabled(false);
        if (page)
        {
            page->addChild(*param);
        }
    }


    {
        OFX::StringParamDescriptor *param = desc.defineStringParam(kMLVfileParamter);
        param->setLabel("Filename");
        param->setHint("Name of the MLV file");
        param->setFilePathExists(true);
        param->setAnimates(false);
        param->setStringType(OFX::eStringTypeFilePath);
        param->setScriptName(kMLVfileParamter);
        desc.addClipPreferencesSlaveParam(*param);
        if (page)
        {
            page->addChild(*param);
        }
    }

    { 
        // linear, sRGB, Adobe, Wide, ProPhoto, XYZ, ACES, DCI-P3, Rec. 2020
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kDebayerType);
        param->setLabel("Debayer");
        param->appendOption("Raw", "", "raw");
        param->appendOption("Linear", "", "linear");
        param->appendOption("VNG", "", "vng");
        param->appendOption("PPG", "", "ppg");
        param->appendOption("AHD", "", "ahd");
        param->appendOption("DCB", "", "dcb");
        //param->appendOption("DHT", "", "dht");
        //param->appendOption("Modifier AHD", "", "mahd");
        param->setHint("The demosaic algorithm to use (CPU only)");
        param->setDefault(1);
        if (page_debayer)
        {
            page_debayer->addChild(*param);
        }
    }

    OpenCLBase::describeInContextCL(desc, context, page_debayer);

    { 
        // raw, sRGB, Adobe, Wide, ProPhoto, XYZ, ACES, DCI-P3, Rec. 2020
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kColorSpaceFormat);
        param->setLabel("Color space");
        param->setHint("Output colorspace (output is always linear)");
        param->appendOption("ACES AP0 - rawtoaces IDT", "", "aces");
        param->appendOption("ACES AP1 - rawtoaces IDT", "", "acesap1");
        param->appendOption("Rec.709", "", "rec709");
        param->appendOption("XYZ-D65", "", "xyz");
        param->setDefault(0);
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }

    {
        OFX::DoubleParamDescriptor* param = desc.defineDoubleParam(kHeadRoom);
        param->setLabel("Headroom");
        param->setHint("ACES Headroom value");
        param->setDefault(4.5);
        param->setRange(1.0, 10.0);
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }

    { 
        // raw, sRGB, Adobe, Wide, ProPhoto, XYZ, ACES, DCI-P3, Rec. 2020
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kHighlightMode);
        param->setLabel("Highlight processing");
        param->setHint("Help to remove pinkish highlights");
        param->appendOption("Clip", "", "clip");
        param->appendOption("Unclip", "", "unclip");
        param->setDefault(0);
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }

    OFX::GroupParamDescriptor* WBgroup = desc.defineGroupParam(kGroupWhiteBalance);
    WBgroup->setLabel("White balance");
    WBgroup->setHint("White balance parameters");
    WBgroup->setEnabled(true);
    WBgroup->setOpen(true);
    if (page_colors) {
        page_colors->addChild(*WBgroup);
    }

    {
        OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kUseSpectralIdt);
        param->setLabel("Use spectral IDT");
        param->setHint("Use spectral sensitivities IDT (AP0 only)");
        param->setDefault(false);
        if (WBgroup)
        {
            param->setParent(*WBgroup);
        }
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }  


    {
        OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kCameraWhiteBalance);
        param->setLabel("Camera white balance");
        param->setHint("Use camera white balance (if disabled, use the color temperature slider)");
        param->setDefault(true);
        if (WBgroup)
        {
            param->setParent(*WBgroup);
        }
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }

    {
        OFX::IntParamDescriptor *param = desc.defineIntParam(kColorTemperature);
        param->setLabel("Color temperature");
        param->setHint("Color temperature in Kelvin");
        param->setRange(2800, 8000);
        param->setDisplayRange(2800, 8000);
        param->setDefault(6500);
        if (WBgroup)
        {
            param->setParent(*WBgroup);
        }
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }


    OFX::GroupParamDescriptor* group = desc.defineGroupParam(kGroupColorAberration);
    group->setLabel("Color aberration correction");
    group->setHint("Color aberration correction parameters (Useful in 3x3 pixel binning mode)");
    group->setEnabled(true);
    group->setOpen(false);
    if (page_colors) {
        page_colors->addChild(*group);
    }
    
    {
        OFX::DoubleParamDescriptor *param = desc.defineDoubleParam(kCACorrectionThreshold);
        param->setLabel("CA Correction Threshold");
        param->setHint("Chromatic Aberration Correction Threshold (1.0 disables correction)");
        param->setRange(0.0, 1.0);
        param->setDisplayRange(0.0, 1.0);
        param->setDefault(1.0);
        if (group)
        {
            param->setParent(*group);
        }
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }

    {
        OFX::IntParamDescriptor *param = desc.defineIntParam(kCACorrectionRadius);
        param->setLabel("CA Correction Radius");
        param->setHint("Chromatic Aberration Correction Radius");
        param->setRange(0, 20);
        param->setDisplayRange(0, 20);
        param->setDefault(0);
        if (group)
        {
            param->setParent(*group);
        }
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }

    { 
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kChromaSmooth);
        param->setLabel("Chroma smoothing");
        param->setHint("Chroma smoothing algorithm (remove chroma noise, slow processing)");
        param->appendOption("None", "", "none");
        param->appendOption("2x2", "2x2 filtering", "cs22");
        param->appendOption("3x3", "3x3 filtering", "cs33");
        param->appendOption("5x5", "3x3 filtering", "cs44");
        param->setDefault(0);
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }

    
    {
        OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kFixFocusPixel);
        param->setLabel("Fix focus pixels");
        param->setHint("Fix focus pixels on mirrorless cameras");
        param->setDefault(true);
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }

    {
        OFX::IntParamDescriptor *param = desc.defineIntParam(kBlackLevel);
        param->setLabel("Black level");
        param->setHint("Raw black level");
        param->setRange(0, 65535);
        param->setDisplayRange(0, 16535);
        param->setDefault(0);
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }

    {
        OFX::IntParamDescriptor *param = desc.defineIntParam(kWhiteLevel);
        param->setLabel("White level");
        param->setHint("Raw white level");
        param->setRange(0, 65535);
        param->setDisplayRange(0, 16535);
        param->setDefault(0);
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }

    {
        OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kResetLevels);
        param->setLabel("Reset levels");
        param->setHint("Reset black and white levels to default values (camera calcualted)");
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }

    {
        OFX::IntParamDescriptor *param = desc.defineIntParam(kBpp);
        param->setLabel("Bpp");
        param->setHint("Bits per pixel");
        param->setEnabled(false);
        param->setDefault(0);
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }

    { 
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kDualIso);
        param->setLabel("Dual ISO mode");
        param->appendOption("Disable", "", "none");
        param->appendOption("High Quality 20bits", "HQ 20bits processing dual ISO", "HQ");
        param->appendOption("Fast preview mode", "Low quality mode for preview", "LQ");
        param->setDefault(0);
        if (page_dualiso)
        {
            page_dualiso->addChild(*param);
        }
    }

    {
        OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kDualIsoFullresBlending);
        param->setLabel("Full resolution blending");
        param->setHint("Full resolution Blending switching on/off");
        param->setDefault(true);
        if (page_dualiso)
        {
            page_dualiso->addChild(*param);
        }
    }

    {
        OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kDualIsoAliasMap);
        param->setLabel("Alias map");
        param->setHint("Alias Map switching on/off");
        param->setDefault(false);
        if (page_dualiso)
        {
            page_dualiso->addChild(*param);
        }
    }

    { 
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kDualIsoAveragingMethod);
        param->setLabel("Averaging method");
        param->appendOption("Disable", "", "none");
        param->appendOption("Amaze", "Amaze interpolation", "amaze");
        param->appendOption("Mean-23", "Mean 23 interpolation", "mean");
        param->setDefault(1);
        if (page_dualiso)
        {
            page_dualiso->addChild(*param);
        }
    }

    {
        OFX::StringParamDescriptor *param = desc.defineStringParam(kAudioFilename);
        param->setLabel("Audio Filename");
        param->setHint("Name of the output audio .wav file");
        param->setDefault("audio.wav");
        param->setFilePathExists(true);
        param->setStringType(OFX::eStringTypeFilePath);
        if (page_audio)
        {
            page_audio->addChild(*param);
        }
    }

    {
        OFX::PushButtonParamDescriptor *param = desc.definePushButtonParam(kAudioExport);
        param->setLabel("Export...");
        param->setHint("Export audio file");
        if (page_audio)
        {
            page_audio->addChild(*param);
        }
    }

    OFX::GroupParamDescriptor* DFgroup = desc.defineGroupParam(kGroupDarkFrame);
    DFgroup->setLabel("Dark frame");
    DFgroup->setHint("Dark frame parameters");
    DFgroup->setEnabled(true);
    DFgroup->setOpen(false);
    if (page_raw) {
        page_raw->addChild(*DFgroup);
    }

    {
        OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kDarkFrameEnable);
        param->setLabel("Use darkframe");
        param->setHint("Use darkframe for noise reduction");
        param->setDefault(false);
        if (DFgroup)
        {
            param->setParent(*DFgroup);
        }
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }

    {
        OFX::StringParamDescriptor *param = desc.defineStringParam(kDarkframefilename);
        param->setLabel("Darkframe Filename");
        param->setHint("Name of the .mlv darkframe file");
        param->setDefault("darkframe.mlv");
        param->setFilePathExists(true);
        param->setStringType(OFX::eStringTypeFilePath);
        if (DFgroup)
        {
            param->setParent(*DFgroup);
        }
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }
    
    {
        OFX::Int2DParamDescriptor *param = desc.defineInt2DParam(kDarkframeRange);
        param->setLabel("Darkframe frame range");
        param->setHint("Darkframe export frame range");
        param->setDefault(0,0);
        if (DFgroup)
        {
            param->setParent(*DFgroup);
        }
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }
    
    {
        OFX::PushButtonParamDescriptor *param = desc.definePushButtonParam(kDarkFrameButton);
        param->setLabel("Generate darkframe");
        param->setHint("Launch darkframe generation");
        if (DFgroup)
        {
            param->setParent(*DFgroup);
        }
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }
}

OFX::ImageEffect *
MLVReaderPluginFactory::createInstance(OfxImageEffectHandle handle,
    OFX::ContextEnum /*context*/)
{
    return new MLVReaderPlugin(handle);
}

void loadPlugin()
{
    OFX::ofxsThreadSuiteCheck();
}

static MLVReaderPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

OFXS_NAMESPACE_ANONYMOUS_EXIT
