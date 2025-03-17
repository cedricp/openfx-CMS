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
#include "CMSMLVReader.h"
#include "../utils.h" 

OFXS_NAMESPACE_ANONYMOUS_ENTER


bool CMSMLVReaderPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!kSupportsRenderScale && ((args.renderScale.x != 1.) || (args.renderScale.y != 1.))) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    if (_mlv_video == nullptr){
        return false;
    }

    rod.x1 = 0;
    rod.x2 = _mlv_video->raw_resolution_x();
    rod.y1 = 0;
    rod.y2 = _mlv_video->raw_resolution_y();
    return true;
}

// the overridden render function
void CMSMLVReaderPlugin::render(const OFX::RenderArguments &args)
{
    if (_mlv_video == nullptr){
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    const int time = floor(args.time+0.5);
    
    bool cam_wb = _cameraWhiteBalance->getValue();
    
    int dng_size = 0;

    OFX::auto_ptr<OFX::Image> dst(_outputClip->fetchImage(args.time));
    
    pthread_mutex_lock(&_mlv_mutex);
    Mlv_video::RawInfo rawInfo;
    rawInfo.temperature = cam_wb ? -1 : _colorTemperature->getValue();
    uint16_t* dng_buffer = _mlv_video->get_dng_buffer(time, rawInfo, dng_size);
    int mlv_width = _mlv_video->raw_resolution_x();
    int mlv_height = _mlv_video->raw_resolution_y();
    mlv_wbal_hdr_t wbobj = _mlv_video->get_wb_object();
    int camid = _mlv_video->get_camid();
    pthread_mutex_unlock(&_mlv_mutex);
    
    if (dng_buffer == nullptr || dng_size == 0){
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum dstBitDepth = _outputClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _outputClip->getPixelComponents();

    assert(OFX_COMPONENTS_OK(dstComponents));

    if (!dst)
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    #define ROL16(v,a) ((v) << (a) | (v) >> (16-(a)))

    if (_debayerType->getValue() == 0){
        pthread_mutex_lock(&_mlv_mutex);
        uint16_t* raw_buffer = _mlv_video->unpacked_buffer(_mlv_video->get_raw_image());
        pthread_mutex_unlock(&_mlv_mutex);

        OfxRectD rodd = _outputClip->getRegionOfDefinition(time, args.renderView);
        int width_img = (int)(rodd.x2 - rodd.x1);
        int height_img = (int)(rodd.y2 - rodd.y1);
    
        for(int y=0; y < height_img; y++) {
            uint16_t* srcPix = raw_buffer + (height_img - 1 -y) * (mlv_width);
            float *dstPix = (float*)dst->getPixelAddress((int)rodd.x1, y+(int)rodd.y1);
            for(int x=0; x < width_img; x++) {
                //uint16_t pix = ROL16(*srcPix, 8);
                float pixel_val = float(*srcPix++) / _maxValue;
                *dstPix++ = pixel_val;
                *dstPix++ = pixel_val;
                *dstPix++ = pixel_val;
            }
        }
        free(raw_buffer);
        free(dng_buffer);
        return;
    }

    
    Dng_processor dng_processor;
    wbobj.kelvin = rawInfo.temperature;
    dng_processor.set_interpolation(_debayerType->getValue()-1);
    dng_processor.set_camera_wb(cam_wb);
    dng_processor.set_wb_coeffs(wbobj);
    dng_processor.set_camid(camid);
    dng_processor.set_highlight(_highlightMode->getValue());
    dng_processor.set_colorspace(_colorSpaceFormat->getValue());

    uint16_t* processed_buffer = dng_processor.get_processed_image((uint8_t*)dng_buffer, dng_size);

    free(dng_buffer);

    OfxRectD rodd = _outputClip->getRegionOfDefinition(time, args.renderView);
    int width_img = (int)(rodd.x2 - rodd.x1);
    int height_img = (int)(rodd.y2 - rodd.y1);

    for(int y=0; y < height_img; y++) {
        uint16_t* srcPix = processed_buffer + (height_img - 1 - y) * (mlv_width * 3);
        float *dstPix = (float*)dst->getPixelAddress((int)rodd.x1, y+(int)rodd.y1);
        for(int x=0; x < width_img; x++) {
            *dstPix++ = float(*srcPix++) / _maxValue;
            *dstPix++ = float(*srcPix++) / _maxValue;
            *dstPix++ = float(*srcPix++) / _maxValue;
        }
    }
}

bool CMSMLVReaderPlugin::getTimeDomain(OfxRangeD& range)
{
    if (_mlv_video == nullptr){
        return false;
    }
    range.min = 1;
    range.max = _mlv_video->frame_count();

    return true;
}

bool CMSMLVReaderPlugin::isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& identityClip, double& identityTime, int& view, std::string& plane)
{
    return false;
}


void CMSMLVReaderPlugin::setMlvFile(std::string file)
{
    if (_mlv_video){
        delete _mlv_video;
        _mlv_video = NULL;
    }
    if (file.empty()){
        return;
    }
    _mlv_video = new Mlv_video(file);
    _mlvfilename = file;
    if (!_mlv_video->valid()){
        delete _mlv_video;
        _mlv_video = nullptr;
    } else {
        _maxValue = pow(2, _mlv_video->bpp());
        OfxPointI tr;
        tr.x = 0;
        tr.y = _mlv_video->frame_count();
        _timeRange->setValue(tr);
    }
}

void CMSMLVReaderPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    if (!_mlv_video || !_mlv_video->valid()){
        return;
    }
    OfxRectI format;
    format.x1 = 0;
    format.x2 = _mlv_video->raw_resolution_x();
    format.y1 = 0;
    format.y2 = _mlv_video->raw_resolution_y();

    // output is continuous
    double par = 1.;
    clipPreferences.setPixelAspectRatio(*_outputClip, par);
    clipPreferences.setOutputFormat(format);
    // MLV clip is a video stream
    clipPreferences.setOutputFrameVarying(true);
}

void CMSMLVReaderPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
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
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderTwiceAlways(false);
    desc.setRenderThreadSafety(OFX::kRenderThreadSafety);
    //desc.getPropertySet().propSetInt(kOfxImageEffectInstancePropSequentialRender, 2, false);
#ifdef OFX_EXTENSIONS_NATRON
    desc.setChannelSelector(OFX::ePixelComponentRGB);
#endif

    OFX::generatorDescribe(desc);
}

void CMSMLVReaderPlugin::changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName)
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

void CMSMLVReaderPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
    OFX::ContextEnum context)
{
    // there has to be an input clip
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);

    srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setOptional(true);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGB);
    dstClip->setSupportsTiles(kSupportsTiles);

    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");
    OFX::PageParamDescriptor *page_debayer = desc.definePageParam("Debayering");
    OFX::PageParamDescriptor *page_colors = desc.definePageParam("Colors");

    {
        OFX::Int2DParamDescriptor *param = desc.defineInt2DParam(kTimeRange);
        //desc.addClipPreferencesSlaveParam(*param);
        param->setLabel("Time range");
        param->setHint("The video time range");
        param->setDefault(0, 0);
        param->setEnabled(false);
        if (page)
        {
            page->addChild(*param);
        }
    }

    {
        OFX::StringParamDescriptor *param = desc.defineStringParam(kMLVfileParamter);
        //desc.addClipPreferencesSlaveParam(*param);
        param->setLabel("Filename");
        param->setHint("Name of the MLV file");
        param->setDefault("");
        param->setFilePathExists(true);
        param->setStringType(OFX::eStringTypeFilePath);
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
        param->appendOption("DHT", "", "dht");
        param->appendOption("Modifier AHD", "", "mahd");
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
        param->appendOption("Raw", "", "linear");
        param->appendOption("Linear", "", "raw");
        param->appendOption("sRGB", "", "srgb");
        param->appendOption("Adobe", "", "adobe");
        param->appendOption("Wide", "", "wide");
        param->appendOption("ProPhoto", "", "prophoto");
        param->appendOption("XYZ", "", "xyz");
        param->appendOption("ACES AP0", "", "aces");
        param->appendOption("DCI-P3", "", "dcip3");
        param->appendOption("Rec.2020", "", "rec2020");
        param->setDefault(1);
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
        param->setHint("Use camera white balance");
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
}

OFX::ImageEffect *
CMSMLVReaderPluginFactory::createInstance(OfxImageEffectHandle handle,
    OFX::ContextEnum /*context*/)
{
    return new CMSMLVReaderPlugin(handle);
}

static CMSMLVReaderPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

OFXS_NAMESPACE_ANONYMOUS_EXIT
