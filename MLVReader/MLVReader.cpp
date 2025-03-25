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

#include <cmath>
#include <climits>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <stddef.h>
extern "C"{
#include <dng/dng.h>
}
#include "ofxOpenGLRender.h"
#include "MLVReader.h"
#include "../utils.h" 

//#define CL_TESTING 1


#define CLAMP(A, L, H) ((A) > (L) ? ((A) < (H) ? (A) : (H)) : (L))
#define ROUNDUP(a, n) ((a) % (n) == 0 ? (a) : ((a) / (n)+1) * (n))

typedef struct opencl_local_buffer_t
{
  const int xoffset;
  const int xfactor;
  const int yoffset;
  const int yfactor;
  const size_t cellsize;
  const size_t overhead;
  int sizex;  // initial value and final values after optimization
  int sizey;  // initial value and final values after optimization
} opencl_local_buffer_t;

static int _nextpow2(const int n)
{
  int k = 1;
  while(k < n)
    k <<= 1;
  return k;
}

int opencl_get_max_work_item_sizes(cl::Device dev,
    std::vector<size_t>& sizes)
{
    int err;
    sizes = dev.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>(&err);
    return err;
}

int opencl_get_work_group_limits(cl::Device dev,
    std::vector<size_t>& sizes,
    size_t& workgroupsize,
    unsigned long& localmemsize)
{
    cl_int err;

    localmemsize = dev.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>(&err);

    if(err != CL_SUCCESS) return err;

    workgroupsize = dev.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>(&err);

    if(err != CL_SUCCESS) return err;

    return opencl_get_max_work_item_sizes(dev, sizes);
}

int opencl_get_kernel_work_group_size(cl::Device dev,
    cl::Kernel ker,
    size_t& kernelworkgroupsize)
{
    cl_int err;
    kernelworkgroupsize = ker.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(dev, &err);
    return err;
}

int opencl_local_buffer_opt(cl::Device dev,
    cl::Kernel ker,
    opencl_local_buffer_t *factors)
{
    std::vector<size_t> maxsizes(3);     // the maximum dimensions for a work group
    size_t workgroupsize = 0;       // the maximum number of items in a work group
    unsigned long localmemsize = 0; // the maximum amount of local memory we can use
    size_t kernelworkgroupsize = 0; // the maximum amount of items in
        // work group for this kernel

    int *blocksizex = &factors->sizex;
    int *blocksizey = &factors->sizey;

    // initial values must be supplied in sizex and sizey.
    // we make sure that these are a power of 2 and lie within reasonable limits.
    *blocksizex = CLAMP(_nextpow2(*blocksizex), 1, 1 << 16);
    *blocksizey = CLAMP(_nextpow2(*blocksizey), 1, 1 << 16);

    if(opencl_get_work_group_limits(dev, maxsizes, workgroupsize, localmemsize) == CL_SUCCESS
                                    && opencl_get_kernel_work_group_size
                                    (dev, ker, kernelworkgroupsize) == CL_SUCCESS)
    {
        while(maxsizes[0] < *blocksizex
            || maxsizes[1] < *blocksizey
            || localmemsize < ((factors->xfactor * (*blocksizex) + factors->xoffset) *
            (factors->yfactor * (*blocksizey) + factors->yoffset))
            * factors->cellsize + factors->overhead
            || workgroupsize < (size_t)(*blocksizex) * (*blocksizey)
            || kernelworkgroupsize < (size_t)(*blocksizex) * (*blocksizey))
        {
            if(*blocksizex == 1 && *blocksizey == 1)
            {
            printf(
            "[opencl_local_buffer_opt] no valid resource limits for curent device");
            return FALSE;
        }

        if(*blocksizex > *blocksizey)
        *blocksizex >>= 1;
        else
        *blocksizey >>= 1;
        }
    }
    else
    {
        printf(
        "can not identify"
        " resource limits for current device");
        return FALSE;
    }

    return TRUE;
}



OFXS_NAMESPACE_ANONYMOUS_ENTER

static std::vector<cl::Device> g_cldevices;

enum ColorSpaceFormat {
        ACES_AP0,
        ACES_AP1,
        REC709,
        XYZ
};

const float xyzD50_rec709D65[9] = {
    3.2404542, -1.5371385, -0.4985314,
    -0.9692660 , 1.8760108, 0.0415560,
     0.0556434, -0.2040259, 1.0572252
};

inline void matrix_vector_mult(const float *mat, const float *vec, float *result, int rows, int cols)
{
    for (int i = 0; i < rows; i++)
    {
        float res = 0.0;
        for (int j = 0; j < cols; j++)
        {
            res += mat[i*cols+j] * vec[j];
        }
        result[i] = res;
    }
}

bool MLVReaderPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!kSupportsRenderScale && ((args.renderScale.x != 1.) || (args.renderScale.y != 1.))) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

#ifdef CL_TESTING
    rod.x1 = 0;
    rod.x2 = 1920;
    rod.y1 = 0;
    rod.y2 = 1080;
    return true;
#endif

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

void MLVReaderPlugin::renderCLTest(OFX::Image* dst, int width, int height)
{
    int cl_dev = _openCLDevices->getValue();
    if (cl_dev >= g_cldevices.size()){
        setPersistentMessage(OFX::Message::eMessageError, "", std::string("Bad OpenCL device"));
        return;
    }
    clearPersistentMessage();

    _current_cldevice = g_cldevices[cl_dev];
    cl::Image2D out(_current_clcontext, CL_MEM_WRITE_ONLY, cl::ImageFormat(CL_RGBA, CL_FLOAT), width, height, 0, NULL);

    cl::Kernel kernel(_current_clprogram, "test_pattern");
    kernel.setArg(0, out);

    cl::Event timer;
    cl::array<size_t, 3> origin = {0,0,0};
    cl::array<size_t, 3> size = {(size_t)width, (size_t)height, 1};
    
    // Render
    cl::CommandQueue queue = cl::CommandQueue(_current_clcontext, _current_cldevice);
    queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(width, height), cl::NullRange, NULL, &timer);
    timer.wait();
    queue.enqueueReadImage(out, CL_TRUE, origin, size, 0, 0, (float*)dst->getPixelData());
    queue.finish();
}

void MLVReaderPlugin::renderCL(OFX::Image* dst, Mlv_video* mlv_video, int time)
{
    int cl_dev = _openCLDevices->getValue();
    if (cl_dev >= g_cldevices.size()){
        setPersistentMessage(OFX::Message::eMessageError, "", std::string("Bad OpenCL device for rendering"));
        return;
    }


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
    uint16_t* dng_buffer = mlv_video->get_dng_buffer(time, rawInfo, dng_size);
    uint16_t* raw_buffer = mlv_video->get_unpacked_raw_buffer();
    
    int32_t wbal[6];
    mlv_wbal_hdr_t wbobj = mlv_video->get_wb_object();
    wbobj.wb_mode = WB_KELVIN;
    wbobj.kelvin = _colorTemperature->getValue();
    ::get_white_balance(wbobj, wbal, camid);
    float wbrgb[4] = {float(wbal[1]) / 1000000.f, float(wbal[3]) / 1000000.f, float(wbal[5]) / 1000000.f, 1.f};
    float ratio = *std::max_element(wbrgb, wbrgb+3) / *std::min_element(wbrgb, wbrgb+3);

    wbrgb[0] *= ratio;
    wbrgb[1] *= ratio;
    wbrgb[2] *= ratio;


    int width = mlv_video->raw_resolution_x();
    int height = mlv_video->raw_resolution_y();

    uint32_t black_level = mlv_video->black_level();
    uint32_t white_level = mlv_video->white_level();

    _current_cldevice = g_cldevices[cl_dev];
    cl::Image2D img_in(_current_clcontext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_R, CL_UNSIGNED_INT16), width, height, 0, raw_buffer);
    cl::Image2D img_out(_current_clcontext, CL_MEM_WRITE_ONLY, cl::ImageFormat(CL_RGBA, CL_FLOAT), width, height, 0, NULL);
    cl::Image2D img_tmp(_current_clcontext, CL_MEM_READ_WRITE, cl::ImageFormat(CL_RGBA, CL_FLOAT), width, height, 0, NULL);
    
    cl::Event timer;
    cl::CommandQueue queue = cl::CommandQueue(_current_clcontext, _current_cldevice);
    uint32_t filter = 0x94949494;
    // {
    //     // Process borders
    //     opencl_local_buffer_t locopt
    //     = (opencl_local_buffer_t){  .xoffset = 2*1, .xfactor = 1, .yoffset = 2*1, .yfactor = 1,
    //                                 .cellsize = 4 * sizeof(float), .overhead = 0,
    //                                 .sizex = 1 << 8, .sizey = 1 << 8 };
    //     cl::Kernel kernel_demosaic_border(_current_clprogram, "border_interpolate");
    //     if (!opencl_local_buffer_opt(_current_cldevice, kernel_demosaic_border, &locopt)){
    //         setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Invalid work dimension (border_interpolate)"));
    //         return;
    //     }

    //     kernel_demosaic_border.setArg(0, img_in);
    //     kernel_demosaic_border.setArg(1, img_tmp);
    //     kernel_demosaic_border.setArg(2, width);
    //     kernel_demosaic_border.setArg(3, height);
    //     kernel_demosaic_border.setArg(4, filter);
    //     kernel_demosaic_border.setArg(5, 3);
    //     kernel_demosaic_border.setArg(6, black_level);
    //     kernel_demosaic_border.setArg(7, white_level);

    //     cl::NDRange sizes(width, height);
        
    //     queue.enqueueNDRangeKernel(kernel_demosaic_border, cl::NullRange, sizes, cl::NullRange, NULL, &timer);
    //     timer.wait();
    // }

    {
        // Process green channel
        opencl_local_buffer_t locopt
        = (opencl_local_buffer_t){  .xoffset = 2*3, .xfactor = 1, .yoffset = 2*3, .yfactor = 1,
                                    .cellsize = sizeof(float) * 1, .overhead = 0,
                                    .sizex = 1 << 8, .sizey = 1 << 8 };
        cl::Kernel kernel_demosaic_green(_current_clprogram, "ppg_demosaic_green");
        if (!opencl_local_buffer_opt(_current_cldevice, kernel_demosaic_green, &locopt)){
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
        
        queue.enqueueNDRangeKernel(kernel_demosaic_green, cl::NullRange, sizes, local, NULL, &timer);
        timer.wait();
    }

    {
        // Process red/blue channel
        opencl_local_buffer_t locopt
        = (opencl_local_buffer_t){  .xoffset = 2*1, .xfactor = 1, .yoffset = 2*1, .yfactor = 1,
                                    .cellsize = 4 * sizeof(float), .overhead = 0,
                                    .sizex = 1 << 8, .sizey = 1 << 8 };
        cl::Kernel kernel_demosaic_redblue(_current_clprogram, "ppg_demosaic_redblue");
        if (!opencl_local_buffer_opt(_current_cldevice, kernel_demosaic_redblue, &locopt)){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Invalid work dimension (red/blue)"));
            return;
        }

        cl::NDRange sizes( ROUNDUP(width, locopt.sizex), ROUNDUP(height, locopt.sizey) );
        cl::NDRange local( locopt.sizex, locopt.sizey );

        kernel_demosaic_redblue.setArg(0, img_tmp);
        kernel_demosaic_redblue.setArg(1, img_out);
        kernel_demosaic_redblue.setArg(2, width);
        kernel_demosaic_redblue.setArg(3, height);
        kernel_demosaic_redblue.setArg(4, filter);
        kernel_demosaic_redblue.setArg(5, wbrgb[0]);
        kernel_demosaic_redblue.setArg(6, wbrgb[1]);
        kernel_demosaic_redblue.setArg(7, wbrgb[2]);
        kernel_demosaic_redblue.setArg(8, sizeof(float) * 4 * (locopt.sizex + 2) * (locopt.sizey + 2), nullptr);
        
        queue.enqueueNDRangeKernel(kernel_demosaic_redblue, cl::NullRange, sizes, local, NULL, &timer);
        timer.wait();
    }
    clearPersistentMessage();

    // Fetch result from GPU
    cl::array<size_t, 3> origin = {0,0,0};
    cl::array<size_t, 3> size = {(size_t)width, (size_t)height, 1};
    queue.enqueueReadImage(img_out, CL_TRUE, origin, size, 0, 0, (float*)dst->getPixelData());
    queue.finish();
}

// the overridden render function
void MLVReaderPlugin::render(const OFX::RenderArguments &args)
{
#ifdef CL_TESTING
    {
        OFX::auto_ptr<OFX::Image> dst(_outputClip->fetchImage(args.time));
        renderCLTest(dst.get(), 1920, 1080);
        return;
    }
#endif
    Mlv_video *mlv_video = nullptr;

    {
        if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return;
        for (int i = 0; i < _mlv_video.size(); ++i){
            if (!_mlv_video[i]->locked()){
                mlv_video = _mlv_video[i];
                _mlv_video[i]->lock();
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

    OfxRectD rodd = _outputClip->getRegionOfDefinition(time, args.renderView);
    int width_img = (int)(rodd.x2 - rodd.x1);
    int height_img = (int)(rodd.y2 - rodd.y1);

    if (_debayerType->getValue() == 0){
        // Extract raw buffer - No processing (debug)
        uint16_t* raw_buffer = mlv_video->get_unpacked_raw_buffer();
    
        for(int y=0; y < height_img; y++) {
            uint16_t* srcPix = raw_buffer + (height_img - 1 -y) * (mlv_width);
            float *dstPix = (float*)dst->getPixelAddress((int)rodd.x1, y+(int)rodd.y1);
            for(int x=0; x < width_img; x++) {
                float pixel_val = float(*srcPix++) / maxval;
                *dstPix++ = pixel_val;
                *dstPix++ = pixel_val;
                *dstPix++ = pixel_val;
                *dstPix++ = 1.f;
            }
        }
        // Release MLV reader
        mlv_video->unlock();
    } else if (_useOpenCL->getValue()){
        // float* fltbuff = (float*)malloc(sizeof(float)*mlv_width*mlv_height);
        // for(int y=0; y < height_img; y++) {
            //     uint16_t* srcPix = raw_buffer + (y) * (mlv_width);
            //     float *dstPix = fltbuff + (y) * (mlv_width);
            //     for(int x=0; x < width_img; x++) {
            //         *dstPix++ = float(*srcPix++) / maxval;
            //     }
            // }
            // Release MLV reader
        renderCL(dst.get(), mlv_video, time);
        mlv_video->unlock();
        //renderCLTest(dst.get(), mlv_width, mlv_height);
        //free(fltbuff);
    } else {
        Mlv_video::RawInfo rawInfo;
        rawInfo.dual_iso_mode = _dualIsoMode->getValue();
        rawInfo.chroma_smooth = _chromaSmooth->getValue();
        rawInfo.fix_focuspixels = _fixFocusPixel->getValue();
        rawInfo.dualisointerpolation = _dualIsoAveragingMethod->getValue(); 
        rawInfo.dualiso_fullres_blending = _dualIsoFullresBlending->getValue();
        rawInfo.dualiso_aliasmap = _dualIsoAliasMap->getValue();
        rawInfo.temperature = cam_wb ? -1 : _colorTemperature->getValue();
        rawInfo.darkframe_file = _mlv_darkframefilename->getValue();
        rawInfo.darkframe_enable = std::filesystem::exists(rawInfo.darkframe_file);
        uint16_t* dng_buffer = mlv_video->get_dng_buffer(time, rawInfo, dng_size);

        mlv_wbal_hdr_t wbobj = mlv_video->get_wb_object();
        int camid = mlv_video->get_camid();
        // Release MLV reader
        mlv_video->unlock();

        if (dng_buffer == nullptr || dng_size == 0){
            OFX::throwSuiteStatusException(kOfxStatFailed);
        }

        // Note : Libraw needs to be compiled with multithreading (reentrant) support and no OpenMP support
        int colorspace = _colorSpaceFormat->getValue();
        Dng_processor dng_processor;
        wbobj.kelvin = rawInfo.temperature;
        dng_processor.set_interpolation(_debayerType->getValue()-1);
        dng_processor.set_camera_wb(cam_wb);
        dng_processor.set_wb_coeffs(wbobj);
        dng_processor.set_camid(camid);
        dng_processor.set_highlight(_highlightMode->getValue());
        dng_processor.setAP1IDT(colorspace == ACES_AP1);
        bool apply_wb = colorspace > ACES_AP1;

        // Get raw buffer in XYZ-D50 colorspace
        uint16_t* processed_buffer = dng_processor.get_processed_image((uint8_t*)dng_buffer, dng_size, apply_wb);
        free(dng_buffer);

        // Compute colorspace matrix and adjust white balance parameters
        float idt_matrix[9] = {0};
        float use_matrix = true;
        float ratio = dng_processor.get_wbratio();
        if(colorspace == ACES_AP0){
            memcpy(idt_matrix, dng_processor.get_idt_matrix(), 9*sizeof(float));
        } else if (colorspace == ACES_AP1){
            memcpy(idt_matrix, dng_processor.get_idt_matrix(), 9*sizeof(float));
        } else if (colorspace == REC709){
            memcpy(idt_matrix, xyzD50_rec709D65, 9*sizeof(float));
            for(int i=0; i<9; ++i) idt_matrix[i] *= ratio; 
        } else {
            use_matrix = false;
            idt_matrix[0] = idt_matrix[4] = idt_matrix [8] = 1;
        }


        for(int y=0; y < height_img; y++) {
            uint16_t* srcPix = processed_buffer + (height_img - 1 - y) * (mlv_width * 3);
            float *dstPix = (float*)dst->getPixelAddress((int)rodd.x1, y+(int)rodd.y1);
            for(int x=0; x < width_img; x++) {
                float in[3];
                in[0] = float(*srcPix++) / _maxValue;
                in[1] = float(*srcPix++) / _maxValue;
                in[2] = float(*srcPix++) / _maxValue;
                if (use_matrix){
                    matrix_vector_mult(idt_matrix, in, dstPix, 3, 3);
                } else {
                    dstPix[0]=in[0];
                    dstPix[1]=in[1];
                    dstPix[2]=in[2];
                    dstPix[3]=1.f;
                }
                dstPix+=4;
            }
        }
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
    return false;
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
#ifdef CL_TESTING
{
    OfxRectI format;
    format.x1 = 0;
    format.x2 = 1920;
    format.y1 = 0;
    format.y2 = 1080;

    double par = 1;
    clipPreferences.setPixelAspectRatio(*_outputClip, par);
    clipPreferences.setOutputFormat(format);
    return;
}
#endif
    // MLV clip is a video stream
    clipPreferences.setOutputFrameVarying(true);

    if (_mlv_video.size() == 0){
        return;
    }
    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return;
    OfxRectI format;
    format.x1 = 0;
    format.x2 = _mlv_video[0]->raw_resolution_x();
    format.y1 = 0;
    format.y2 = _mlv_video[0]->raw_resolution_y();

    double par = 1;
    clipPreferences.setPixelAspectRatio(*_outputClip, par);
    clipPreferences.setOutputFormat(format);

    _gThreadHost->mutexUnLock(_videoMutex);
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

    bool use_opencl = _useOpenCL->getValue();
    if (paramName == kUseOpenCL || paramName == kOpenCLDevice){
        _openCLDevices->setEnabled(use_opencl);
        if (use_opencl){
            setupOpenCL();
        }
    }
}

bool MLVReaderPlugin::setupOpenCL()
{
    if (g_cldevices.empty()){
        return false;
    }

    std::string programpath = getPluginFilePath() + "/Contents/Resources/debayer_ppg.cl";
    std::ifstream programfile;
    programfile.open(programpath.c_str());
    std::ostringstream programtext;
    programtext << programfile.rdbuf();
    programfile.close();

    if (programtext.str().empty()){
        setPersistentMessage(OFX::Message::eMessageError, "", "Failed to load OpenCL program");
        return false;
    }

    clearPersistentMessage();

    int cl_dev = _openCLDevices->getValue();
    _current_cldevice = g_cldevices[cl_dev];

    cl::Platform platform(_current_cldevice.getInfo<CL_DEVICE_PLATFORM>());
    _current_clcontext = cl::Context(_current_cldevice);

    cl_int err;
    _current_clprogram = cl::Program(_current_clcontext, programtext.str(), true, &err);
    if (err != CL_SUCCESS){
        std::string errlog = _current_clprogram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(_current_cldevice);
        printf("OpenCL Error :\n%s\n", errlog.c_str());
        setPersistentMessage(OFX::Message::eMessageError, "", "Failed to create program");
        return false;
    }

    clearPersistentMessage();

    printf("OpenCL OK!\n");

    return true;
}

void MLVReaderPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
    OFX::ContextEnum context)
{
    // There has to be an input clip
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);

    srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
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

    { 
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kUseOpenCL);
        param->setLabel("OpenCL acceleration");
        param->setDefault(0);
        if (page_debayer)
        {
            page_debayer->addChild(*param);
        }
    }

    { 
        // linear, sRGB, Adobe, Wide, ProPhoto, XYZ, ACES, DCI-P3, Rec. 2020
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kOpenCLDevice);
        param->setLabel("OpenCL device");
        for (auto cldev : g_cldevices){
            param->appendOption(cldev.getInfo<CL_DEVICE_NAME>(), "", cldev.getInfo<CL_DEVICE_NAME>());
        }
        param->setDefault(0);
        param->setEnabled(false);
        if (page_debayer)
        {
            page_debayer->addChild(*param);
        }
    }

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
        param->setRange(800, 8500);
        param->setDisplayRange(800, 8500);
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

    g_cldevices.clear();
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    for (size_t i=0; i < platforms.size(); i++){
        std::vector<cl::Device> platformDevices;
        platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &platformDevices);
        for (size_t i=0; i < platformDevices.size(); i++) {
            g_cldevices.push_back(platformDevices[i]);
        }
    }
}

static MLVReaderPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

OFXS_NAMESPACE_ANONYMOUS_EXIT
