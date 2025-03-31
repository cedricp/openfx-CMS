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
#define IDT_NOGNG
#include <RawLib/idt/dng_idt.h>
#undef IDT_NOGNG

#include <cmath>
#include <climits>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <stddef.h>
#include "ofxOpenGLRender.h"
#include "../utils.h" 
#include "mathutils.h"

extern "C"{
#include <dng/dng.h>
}

OFXS_NAMESPACE_ANONYMOUS_ENTER

enum ColorSpaceFormat {
        ACES_AP0,
        ACES_AP1,
        REC709,
        XYZ
};

class ColorProcessor
    : public OFX::ImageProcessor
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
        for (int y = procWindow.y1; y < procWindow.y2; y++)
        {
            if (y >= raw_height) break;
            float *dstPix = static_cast<float*>(_dstImg->getPixelAddress(procWindow.x1, y) );
            uint16_t* srcPix = raw_buffer + (raw_height - 1 - y) * (raw_width * 3) + (procWindow.x1 * 3);
            for (int x = procWindow.x1; x < procWindow.x2; ++x)
            {
                if (x > raw_width) break;
                float in[3] = { float(*srcPix++) * scale, float(*srcPix++) * scale, float(*srcPix++) * scale };
                dstPix[0] = idt_matrix[0]*in[0] + idt_matrix[1]*in[1] + idt_matrix[2]*in[2];
                dstPix[1] = idt_matrix[3]*in[0] + idt_matrix[4]*in[1] + idt_matrix[5]*in[2];
                dstPix[2] = idt_matrix[6]*in[0] + idt_matrix[7]*in[1] + idt_matrix[8]*in[2];
                dstPix[3] = 1.f;
                dstPix+=4;
            }
        }
    }

    float idt_matrix[9];
    OFX::Image *srcImg;
    float scale;
    uint16_t *raw_buffer;
    int raw_width, raw_height;
};

bool MLVReaderPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!kSupportsRenderScale && ((args.renderScale.x != 1.) || (args.renderScale.y != 1.))) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    if (_mlv_video.empty()){
        return false;
    }

    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return false;
    rod.x1 = 0;
    rod.x2 = _mlv_video[0]->raw_resolution_x();
    rod.y1 = 0;
    rod.y2 = _mlv_video[0]->raw_resolution_y();
    _gThreadHost->mutexUnLock(_videoMutex);
    return true;
}

// the overridden render function
void MLVReaderPlugin::render(const OFX::RenderArguments &args)
{
    Mlv_video *mlv_video = nullptr;
    {
        if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return;
        for (int i = 0; i < _mlv_video.size(); ++i){
            if (!_mlv_video[i]->locked()){
                _mlv_video[i]->lock();
                mlv_video = _mlv_video[i];
                break;
            }
        }
        _gThreadHost->mutexUnLock(_videoMutex);
    }

    if (mlv_video == nullptr){
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }
    
    const int time = floor(args.time+0.5);
    
    bool cam_wb = _cameraWhiteBalance->getValue();
    int dng_size = 0;

    OFX::BitDepthEnum dstBitDepth = _outputClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _outputClip->getPixelComponents();
    assert(OFX_COMPONENTS_OK(dstComponents));

    OFX::auto_ptr<OFX::Image> dst(_outputClip->fetchImage(args.time));

    if (!dst)
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    float maxval = _maxValue;
    int mlv_width = mlv_video->raw_resolution_x();
    int mlv_height = mlv_video->raw_resolution_y();

    OfxRectI renderWin = args.renderWindow;
    int width_img = (int)(renderWin.x2 - renderWin.x1);
    int height_img = (int)(renderWin.y2 - renderWin.y1);

    if (_debayerType->getValue() == 0){
        // Extract raw buffer - No processing (debug)
        Mlv_video::RawInfo  info;
        mlv_video->get_dng_buffer(time, info, dng_size, true);
        uint16_t* raw_buffer = mlv_video->postprocecessed_raw_buffer();

        for(int y=renderWin.y1; y < renderWin.y2; y++) {
            uint16_t* srcPix = raw_buffer + ((width_img) * (height_img - 1 - y)) + ((int)renderWin.x1 * 4);
            float *dstPix = (float*)dst->getPixelAddress((int)renderWin.x1, y+(int)renderWin.y1);
            for(int x=renderWin.x1; x < renderWin.x2; x++) {
                float pixel_val = float(*srcPix++) / maxval;
                *dstPix++ = pixel_val;
                *dstPix++ = pixel_val;
                *dstPix++ = pixel_val;
                *dstPix++ = 1.f;
            }
        }
        // Release MLV reader
        mlv_video->unlock();
    } else if (getUseOpenCL()){
        renderCL(dst.get(), mlv_video, time);
        mlv_video->unlock();
    } else {
        renderCPU(args, dst.get(), mlv_video, cam_wb, dng_size, time, mlv_height, mlv_width);
    }
}

void MLVReaderPlugin::renderCL(OFX::Image* dst, Mlv_video* mlv_video, int time)
{
    Mlv_video::RawInfo rawInfo;
    int dng_size = 0;
    int camid = mlv_video->get_camid();
    rawInfo.dual_iso_mode = _dualIsoMode->getValue();
    rawInfo.chroma_smooth = _chromaSmooth->getValue();
    rawInfo.fix_focuspixels = _fixFocusPixel->getValue();
    rawInfo.dualisointerpolation = _dualIsoAveragingMethod->getValue(); 
    rawInfo.dualiso_fullres_blending = _dualIsoFullresBlending->getValue();
    rawInfo.dualiso_aliasmap = _dualIsoAliasMap->getValue();
    rawInfo.darkframe_file = _mlv_darkframefilename->getValue();
    rawInfo.darkframe_enable = std::filesystem::exists(rawInfo.darkframe_file);
    mlv_video->get_dng_buffer(time, rawInfo, dng_size, true);
    uint16_t* raw_buffer = mlv_video->postprocecessed_raw_buffer();
    
    float wbrgb[4];
    float wb_compensation;
    mlv_video->get_white_balance_coeffs(_colorTemperature->getValue(), wbrgb, wb_compensation, _cameraWhiteBalance->getValue());

    float cam_matrix[18] = {0};
    cam_matrix[0] = cam_matrix[4] = cam_matrix [8] = 1;

    int colorspace = _colorSpaceFormat->getValue();
    
    uint32_t black_level = mlv_video->black_level();
    uint32_t white_level = mlv_video->white_level();

    if(colorspace <= ACES_AP1){
        float xyz[9], cam2rec709[9], xyz2cam[9];
        mlv_video->get_camera_matrix2f(xyz2cam);
        get_matrix_cam2rec709(xyz2cam, cam2rec709);
        mat_mat_mult(rec709toxyzD50, cam2rec709, cam_matrix);

        DNGIdt::DNGIdt idt(mlv_video, wbrgb);
        idt.getDNGIDTMatrix2(cam_matrix + 9, colorspace == ACES_AP1);
    } else if (colorspace == REC709){
        float cam2rec709[9], xyz2cam[9];
        mlv_video->get_camera_matrix2f(xyz2cam);
        get_matrix_cam2rec709(xyz2cam, cam_matrix+9);
    } else {
        float xyz[9], cam2rec709[9], xyz2cam[9];
        mlv_video->get_camera_matrix2f(xyz2cam);
        get_matrix_cam2rec709(xyz2cam, cam2rec709);
        mat_mat_mult(rec709toxyzD50, cam2rec709, cam_matrix+9);
    }

    int width = mlv_video->raw_resolution_x();
    int height = mlv_video->raw_resolution_y();

    cl::Image2D img_in(getCurrentCLContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_R, CL_UNSIGNED_INT16), width, height, 0, raw_buffer);
    cl::Image2D img_out(getCurrentCLContext(), CL_MEM_WRITE_ONLY, cl::ImageFormat(CL_RGBA, CL_FLOAT), width, height, 0, NULL);
    cl::Image2D img_tmp(getCurrentCLContext(), CL_MEM_READ_WRITE, cl::ImageFormat(CL_RGBA, CL_FLOAT), width, height, 0, NULL);
    
    cl::Event timer;
    cl::CommandQueue queue = cl::CommandQueue(getCurrentCLContext(), getCurrentCLDevice());

    // Standard Canon filter (RGGB)
    uint32_t filter = 0x94949494;
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
        kernel_demosaic_border.setArg(4, filter);
        kernel_demosaic_border.setArg(5, 3);
        kernel_demosaic_border.setArg(6, black_level);
        kernel_demosaic_border.setArg(7, white_level);
        kernel_demosaic_border.setArg(8, wbrgb[0]);
        kernel_demosaic_border.setArg(9, wbrgb[2]);

        cl::NDRange sizes(width, height);
        
        queue.enqueueNDRangeKernel(kernel_demosaic_border, cl::NullRange, sizes, cl::NullRange, NULL, &timer);
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
        kernel_demosaic_green.setArg(4, filter);
        kernel_demosaic_green.setArg(5, black_level);
        kernel_demosaic_green.setArg(6, white_level);
        kernel_demosaic_green.setArg(7, sizeof(float) * (locopt.sizex + 2*3) * (locopt.sizey + 2*3), nullptr);
        kernel_demosaic_green.setArg(8, wbrgb[0]);
        kernel_demosaic_green.setArg(9, wbrgb[2]);
        
        queue.enqueueNDRangeKernel(kernel_demosaic_green, cl::NullRange, sizes, local, NULL, &timer);
    }

    {
        // Process red/blue channel
        OpenCLLocalBufferStruct locopt
        = (OpenCLLocalBufferStruct){  .xoffset = 2*1, .xfactor = 1, .yoffset = 2*1, .yfactor = 1,
                                    .cellsize = 4 * sizeof(float), .overhead = 0,
                                    .sizex = 1 << 8, .sizey = 1 << 8 };
        cl::Kernel kernel_demosaic_redblue(getProgram("debayer_ppg"), "ppg_demosaic_redblue");
        if (!openCLGetLocalBufferOpt(getCurrentCLDevice(), kernel_demosaic_redblue, &locopt)){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Invalid work dimension (red/blue)"));
            return;
        }

        
        cl::NDRange sizes( ROUNDUP(width, locopt.sizex), ROUNDUP(height, locopt.sizey) );
        cl::NDRange local( locopt.sizex, locopt.sizey );
        
        // matrixbufer [0-8] = forward matrix, [9-17] = idt matrix
        cl::Buffer matrixbuffer(getCurrentCLContext(), CL_MEM_READ_ONLY, sizeof(float) * 18);
        kernel_demosaic_redblue.setArg(0, img_tmp);
        kernel_demosaic_redblue.setArg(1, img_out);
        kernel_demosaic_redblue.setArg(2, width);
        kernel_demosaic_redblue.setArg(3, height);
        kernel_demosaic_redblue.setArg(4, filter);
        kernel_demosaic_redblue.setArg(5, matrixbuffer);
        kernel_demosaic_redblue.setArg(6, sizeof(float) * 4 * (locopt.sizex + 2) * (locopt.sizey + 2), nullptr);
        
        queue.enqueueWriteBuffer(matrixbuffer, CL_TRUE, 0, sizeof(float) * 18, cam_matrix);
        queue.enqueueNDRangeKernel(kernel_demosaic_redblue, cl::NullRange, sizes, local, NULL, &timer);
    }
    clearPersistentMessage();

    // Fetch result from GPU
    cl::array<size_t, 3> origin = {0,0,0};
    cl::array<size_t, 3> size = {(size_t)width, (size_t)height, 1};
    queue.enqueueReadImage(img_out, CL_TRUE, origin, size, 0, 0, (float*)dst->getPixelData());
    queue.finish();
}

void MLVReaderPlugin::renderCPU(const OFX::RenderArguments &args, OFX::Image* dst, Mlv_video* mlv_video, bool cam_wb, int dng_size, int time, int height_img, int width_img)
{
    Mlv_video::RawInfo rawInfo;
    rawInfo.dual_iso_mode = _dualIsoMode->getValue();
    rawInfo.chroma_smooth = _chromaSmooth->getValue();
    rawInfo.fix_focuspixels = _fixFocusPixel->getValue();
    rawInfo.dualisointerpolation = _dualIsoAveragingMethod->getValue(); 
    rawInfo.dualiso_fullres_blending = _dualIsoFullresBlending->getValue();
    rawInfo.dualiso_aliasmap = _dualIsoAliasMap->getValue();
    rawInfo.darkframe_file = _mlv_darkframefilename->getValue();
    rawInfo.darkframe_enable = std::filesystem::exists(rawInfo.darkframe_file);
    uint16_t* dng_buffer = mlv_video->get_dng_buffer(time, rawInfo, dng_size);
    
    int color_temperature = _colorTemperature->getValue();
    
    if (dng_buffer == nullptr || dng_size == 0){
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }

    // Note : Libraw needs to be compiled with multithreading (reentrant) support and no OpenMP support
    int colorspace = _colorSpaceFormat->getValue();
    Dng_processor dng_processor(mlv_video);

    // Release MLV reader
    mlv_video->unlock();

    int highlight_mode = _highlightMode->getValue();
    dng_processor.set_interpolation(_debayerType->getValue()-1);
    dng_processor.set_camera_wb(cam_wb);
    dng_processor.set_highlight(highlight_mode);
    dng_processor.set_color_temperature(color_temperature);
    dng_processor.setAP1IDT(colorspace == ACES_AP1);
    bool apply_wb = colorspace <= ACES_AP1;

    float scale = 1./65535. * (highlight_mode > 0 ? 2.f : 1.f);
    
    // Get raw buffer in XYZ-D50 colorspace
    uint16_t* processed_buffer = dng_processor.get_processed_image((uint8_t*)dng_buffer, dng_size, apply_wb);
    free(dng_buffer);
    
    ColorProcessor processor(*this);
    processor.setDstImg(dst);
    processor.raw_buffer = processed_buffer;
    processor.setRenderWindow(args.renderWindow, args.renderScale);
    processor.scale = scale;
    processor.raw_width = width_img;
    processor.raw_height = height_img;
    compute_colorspace_xform_matrix(processor.idt_matrix, dng_processor);
    processor.process();
}

void MLVReaderPlugin::compute_colorspace_xform_matrix(float idt_matrix[9], Dng_processor& dng_processor)
{
    int colorspace = _colorSpaceFormat->getValue();

    if(colorspace == ACES_AP0){
        memcpy(idt_matrix, dng_processor.get_idt_matrix(), 9*sizeof(float));
    } else if (colorspace == ACES_AP1){
        memcpy(idt_matrix, dng_processor.get_idt_matrix(), 9*sizeof(float));
    } else if (colorspace == REC709){
        memcpy(idt_matrix, xyzD50_rec709D65, 9*sizeof(float));
    } else {
        // XYZD50 -- Return identity matrix
        idt_matrix[0] = idt_matrix[4] = idt_matrix [8] = 1;
    }
}

bool MLVReaderPlugin::getTimeDomain(OfxRangeD& range)
{
    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return false;

    if (_mlv_video.empty()){
        _gThreadHost->mutexUnLock(_videoMutex);
        return false;
    }

    range.min = 1;
    range.max = _mlv_video[0]->frame_count();
    _gThreadHost->mutexUnLock(_videoMutex);

    return true;
}

bool MLVReaderPlugin::isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& identityClip, double& identityTime, int& view, std::string& plane)
{
    return true;
}

void MLVReaderPlugin::setMlvFile(std::string file)
{
    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return;
    for (Mlv_video* mlv : _mlv_video){
        if (mlv){
            // Wait for the videostream to be released by renderer
            while (mlv->locked()){Sleep(10);}
            // Now we're sure no one is using the stream
            delete mlv;
        }
    }
    
    _mlv_video.clear();
    
    // As mlv-lib does not support multi threading
    // because of file operations, I just create
    // multiples instances
    for (int i = 0; i < _numThreads; ++i){
        Mlv_video* mlv_video = new Mlv_video(file);
        _mlvfilename = file;
        if (!mlv_video->valid()){
            delete mlv_video;
        } else {
            if (i == 0){
                _maxValue = pow(2, mlv_video->bpp());
                OfxPointI tr;
                tr.x = 0;
                tr.y = mlv_video->frame_count();
                _timeRange->setValue(tr);
                _mlv_fps->setValue(mlv_video->fps());
            }
            _mlv_video.push_back(mlv_video);
        }
    }
    _gThreadHost->mutexUnLock(_videoMutex);
}

void MLVReaderPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    // MLV clip is a video stream
    
    if (_mlv_video.size() == 0){
        return;
    }
    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return;

    OfxRectI format;
    format.x1 = 0;
    format.x2 = _mlv_video[0]->raw_resolution_x();
    format.y1 = 0;
    format.y2 = _mlv_video[0]->raw_resolution_y();
    
    clipPreferences.setOutputFrameVarying(true);
    clipPreferences.setOutputFormat(format);
    
    clipPreferences.setPixelAspectRatio(*_outputClip, 1);
    clipPreferences.setClipBitDepth(*_outputClip, OFX::eBitDepthFloat);
    clipPreferences.setClipComponents(*_outputClip, OFX::ePixelComponentRGBA);

    clipPreferences.setOutputHasContinuousSamples(true);

    _gThreadHost->mutexUnLock(_videoMutex);
}

void MLVReaderPlugin::changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName)
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
#ifdef OFX_EXTENSIONS_NATRON
    desc.setChannelSelector(OFX::ePixelComponentRGB);
#endif
}

void MLVReaderPlugin::changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName)
{
    if (paramName == kMLVfileParamter)
    {
        std::string filename = _mlvfilename_param->getValue();
        std::string upperfn = filename;
        std::transform(upperfn.begin(), upperfn.end(), upperfn.begin(), ::toupper);
        if (upperfn.find(".MLV") == std::string::npos){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("Unsupported file extension"));
            OFX::throwSuiteStatusException(kOfxStatFailed);
            return;
        }
        clearPersistentMessage();
        if (filename != _mlvfilename){
            setMlvFile(filename);
            _mlvfilename = filename;
        }
    }

    if (paramName == kCameraWhiteBalance){
        _colorTemperature->setEnabled(_cameraWhiteBalance->getValue() == false);
    }

    if (paramName == kDualIso){
        bool enabled = _dualIsoMode->getValue() > 0;
        _dualIsoAliasMap->setEnabled(enabled);
        _dualIsoAveragingMethod->setEnabled(enabled);
        _dualIsoFullresBlending->setEnabled(enabled);
    }

    if (paramName == kAudioExport){
        std::string filename = _mlv_audiofilename->getValue();
        if (filename.empty()) return;
        if (_mlv_video.empty()) return;
        
        if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return;
        Mlv_video  *mlv_video = nullptr;
        for(Mlv_video *mlv: _mlv_video){
            if (!mlv->locked()){
                mlv_video = mlv;
                break;
            }
        }
        if (mlv_video) mlv_video->write_audio(filename);
        _gThreadHost->mutexUnLock(_videoMutex);
    }

    if (paramName == kDarkFrameButon){
        int sf = _darkframeRange->getValue().x;
        int ef = _darkframeRange->getValue().y;
        if (sf >= ef) return;
        std::string filename = _mlv_darkframefilename->getValue();
        if (filename.empty()) return;
        if (_mlv_video.empty()) return;
        
        if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return;
        Mlv_video  *mlv_video = nullptr;
        for(Mlv_video *mlv: _mlv_video){
            if (!mlv->locked()){
                mlv_video = mlv;
                break;
            }
        }
        if (mlv_video) mlv_video->generate_darkframe(filename.c_str(), sf, ef);
        _gThreadHost->mutexUnLock(_videoMutex);
    }

    if (paramName == kDualIso){
        bool enabled = _dualIsoMode->getValue() > 0;
        _dualIsoAliasMap->setEnabled(enabled);
        _dualIsoAveragingMethod->setEnabled(enabled);
        _dualIsoFullresBlending->setEnabled(enabled);
    }

    if (OpenCLBase::changedParamCL(this, args, paramName))
    {
        _debayerType->setEnabled(getUseOpenCL() == false);
        _highlightMode->setEnabled(getUseOpenCL() == false);
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
        param->setDefault("");
        param->setFilePathExists(true);
        param->setStringType(OFX::eStringTypeFilePath);
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
        param->appendOption("ACES AP0 - Raw2Aces IDT", "", "aces");
        param->appendOption("ACES AP1 - Raw2Aces IDT", "", "acesap1");
        param->appendOption("Rec.709", "", "rec709");
        param->appendOption("XYZ-D50", "", "xyz");
        param->setDefault(0);
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }

    { 
        // raw, sRGB, Adobe, Wide, ProPhoto, XYZ, ACES, DCI-P3, Rec. 2020
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kHighlightMode);
        param->setLabel("HIghlight processing");
        param->appendOption("Clip", "", "clip");
        param->appendOption("Unclip", "", "unclip");
        param->appendOption("Blend", "", "blend");
        param->appendOption("Rebuild - 1", "", "rebuild1");
        param->appendOption("Rebuild - 2", "", "rebuild2");
        param->appendOption("Rebuild - 3", "", "rebuild3");
        param->appendOption("Rebuild - 4", "", "rebuild4");
        param->setDefault(1);
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
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }

    {
        OFX::IntParamDescriptor *param = desc.defineIntParam(kColorTemperature);
        param->setLabel("Color temperature");
        param->setRange(2800, 8000);
        param->setDisplayRange(2800, 8000);
        param->setHint("Color temperature in Kelvin");
        param->setDefault(6500);
        if (page_colors)
        {
            page_colors->addChild(*param);
        }
    }

    { 
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kChromaSmooth);
        param->setLabel("Chroma smoothing");
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
        param->setHint("Fix focus pixels");
        param->setDefault(true);
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

    {
        OFX::StringParamDescriptor *param = desc.defineStringParam(kDarkframefilename);
        param->setLabel("Darkframe Filename");
        param->setHint("Name of the .mlv darkframe file");
        param->setDefault("darkframe.mlv");
        param->setFilePathExists(true);
        param->setStringType(OFX::eStringTypeFilePath);
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }
    
    {
        OFX::Int2DParamDescriptor *param = desc.defineInt2DParam(kDarkframeRange);
        param->setLabel("Darkframe frame range");
        param->setHint("Darkframe export frame range");
        if (page_raw)
        {
            page_raw->addChild(*param);
        }
    }
    
    {
        OFX::PushButtonParamDescriptor *param = desc.definePushButtonParam(kDarkFrameButon);
        param->setLabel("Generate darkframe");
        param->setHint("Lauch darkframe generation");
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
