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

#include "ofxOpenGLRender.h"
#include "MLVReader.h"
#include "../utils.h" 


OFXS_NAMESPACE_ANONYMOUS_ENTER

enum ColorSpaceFormat {
        ACES_AP0,
        ACES_AP1,
        REC709,
        XYZ
};

const float xyzd50_rec709_d65[9] = {
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
    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return;
    Mlv_video *mlv_video = nullptr;
    for (int i = 0; i < _mlv_video.size(); ++i){
        if (!_mlv_video[i]->locked()){
            mlv_video = _mlv_video[i];
            _mlv_video[i]->lock();
            break;
        }
    }
    _gThreadHost->mutexUnLock(_videoMutex);

    if (mlv_video == nullptr){
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }
    
    
    bool cam_wb = _cameraWhiteBalance->getValue();
    const int time = floor(args.time+0.5);
    int dng_size = 0;

    Mlv_video::RawInfo rawInfo;
    rawInfo.dual_iso_mode = _dualIsoMode->getValue();
    rawInfo.chroma_smooth = _chromaSmooth->getValue();
    rawInfo.fix_focuspixels = _fixFocusPixel->getValue();
    rawInfo.dualisointerpolation = _dualIsoAveragingMethod->getValue(); 
    rawInfo.dualiso_fullres_blending = _dualIsoFullresBlending->getValue();
    rawInfo.dualiso_aliasmap = _dualIsoAliasMap->getValue();
    rawInfo.temperature = cam_wb ? -1 : _colorTemperature->getValue();
    uint16_t* dng_buffer = mlv_video->get_dng_buffer(time, rawInfo, dng_size);

    int mlv_width = mlv_video->raw_resolution_x();
    int mlv_height = mlv_video->raw_resolution_y();
    mlv_wbal_hdr_t wbobj = mlv_video->get_wb_object();
    int camid = mlv_video->get_camid();

    if (dng_buffer == nullptr || dng_size == 0){
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    
    OFX::BitDepthEnum dstBitDepth = _outputClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _outputClip->getPixelComponents();
    assert(OFX_COMPONENTS_OK(dstComponents));

    OFX::auto_ptr<OFX::Image> dst(_outputClip->fetchImage(args.time));

    if (!dst)
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    float maxval = _maxValue;

    OfxRectD rodd = _outputClip->getRegionOfDefinition(time, args.renderView);
    int width_img = (int)(rodd.x2 - rodd.x1);
    int height_img = (int)(rodd.y2 - rodd.y1);

    if (_debayerType->getValue() == 0){
        uint16_t* raw_buffer = mlv_video->unpacked_raw_buffer(mlv_video->get_raw_image());

    
        for(int y=0; y < height_img; y++) {
            uint16_t* srcPix = raw_buffer + (height_img - 1 -y) * (mlv_width);
            float *dstPix = (float*)dst->getPixelAddress((int)rodd.x1, y+(int)rodd.y1);
            for(int x=0; x < width_img; x++) {
                float pixel_val = float(*srcPix++) / maxval;
                *dstPix++ = pixel_val;
                *dstPix++ = pixel_val;
                *dstPix++ = pixel_val;
            }
        }
        free(raw_buffer);
        free(dng_buffer);
        // Release MLV reader
        mlv_video->unlock();
    } else {
        // mlv_video not accessed anymore
        mlv_video->unlock();

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

        // Compute colorspace matrix and adjust white balance parameters
        float idt_matrix[9] = {0};
        float use_matrix = true;
        float ratio = dng_processor.get_wbratio();
        if(colorspace == ACES_AP0){
            memcpy(idt_matrix, dng_processor.get_idt_matrix(), 9*sizeof(float));
        } else if (colorspace == ACES_AP1){
            memcpy(idt_matrix, dng_processor.get_idt_matrix(), 9*sizeof(float));
        } else if (colorspace == REC709){
            memcpy(idt_matrix, xyzd50_rec709_d65, 9*sizeof(float));
            for(int i=0; i<9; ++i) idt_matrix[i] *= ratio; 
        } else {
            use_matrix = false;
            idt_matrix[0] = idt_matrix[4] = idt_matrix [8] = 1;
        }

        free(dng_buffer);

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
                }
                dstPix+=3;
            }
        }
    }
}

bool MLVReaderPlugin::getTimeDomain(OfxRangeD& range)
{
    if (_mlv_video.empty()){
        return false;
    }

    if (_gThreadHost->mutexLock(_videoMutex) != kOfxStatOK) return false;
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
            delete mlv;
        }
    }

    // Now we're sure no one is using the stream
    
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

    double par = 1.;
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
        if (filename != _mlvfilename){
            setMlvFile(filename);
            _mlvfilename = filename;
        }
    }

    if (paramName == kCameraWhiteBalance){
        _colorTemperature->setEnabled(_cameraWhiteBalance->getValue() == false);
    }
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
    dstClip->addSupportedComponent(OFX::ePixelComponentRGB);
    dstClip->setSupportsTiles(kSupportsTiles);

    // Create pages
    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");
    OFX::PageParamDescriptor *page_raw = desc.definePageParam("Raw processing");
    OFX::PageParamDescriptor *page_dualiso = desc.definePageParam("Dual iso");
    OFX::PageParamDescriptor *page_debayer = desc.definePageParam("Debayering");
    OFX::PageParamDescriptor *page_colors = desc.definePageParam("Colors");

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
}

OFX::ImageEffect *
MLVReaderPluginFactory::createInstance(OfxImageEffectHandle handle,
    OFX::ContextEnum /*context*/)
{
    return new MLVReaderPlugin(handle);
}

static MLVReaderPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

OFXS_NAMESPACE_ANONYMOUS_EXIT
