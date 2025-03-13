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

        return false;
    }
    
    // rod.x1 = 0;
    // rod.x2 = resolution.x;
    // rod.y1 = 0;
    // rod.y2 = resolution.y;
    return true;
}

void CMSMLVReaderPlugin::changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName)
{
    if (paramName == kMLVfileParamter)
    {
        std::string filename = _mlvfilename_param->getValue();
        _mlv_video = new Mlv_video(filename);
    }
}


// the overridden render function
void CMSMLVReaderPlugin::render(const OFX::RenderArguments &args)
{
    if (!_mlv_video || !_mlv_video->valid()){
        return;
    }

    // instantiate the render code based on the pixel depth of the dst clip
    const double time = args.time;
    OFX::BitDepthEnum dstBitDepth = _dstClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _dstClip->getPixelComponents();

    printf(">> time :%f\n", time);

    assert(OFX_COMPONENTS_OK(dstComponents));

    checkComponents(dstBitDepth, dstComponents);

    OFX::auto_ptr<OFX::Image> dst(_dstClip->fetchImage(time));

    if (!dst.get())
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
#ifndef NDEBUG
    if ((dstBitDepth != _dstClip->getPixelDepth()) ||
        (dstComponents != _dstClip->getPixelComponents()))
    {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong depth or components");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    checkBadRenderScaleOrField(dst, args);
#endif

    OfxRectD rodd = _dstClip->getRegionOfDefinition(time, args.renderView);
    int width_img = (int)(rodd.x2 - rodd.x1);
    int height_img = (int)(rodd.y2 - rodd.y1);

    for(int y=0; y < height_img; y++) {
        float *dstPix = (float *)dst->getPixelAddress(rodd.x1, y+rodd.y1);
        for(int x=0; x < width_img; x++) {

        }
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
    clipPreferences.setOutputHasContinuousSamples(false);
    double par = 1.;
    clipPreferences.setPixelAspectRatio(*_dstClip, par);
    clipPreferences.setOutputFormat(format);
}

void CMSMLVReaderPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    desc.setLabel(kPluginName);
    desc.setPluginDescription(kPluginDescription);
    desc.setPluginGrouping(kPluginGrouping);
    desc.addSupportedContext(OFX::eContextGenerator);
    desc.addSupportedContext(OFX::eContextGeneral);
    desc.addSupportedBitDepth(OFX::eBitDepthUShort);
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
#ifdef OFX_EXTENSIONS_NATRON
    desc.setChannelSelector(OFX::ePixelComponentRGB);
#endif

    OFX::generatorDescribe(desc);
}

void CMSMLVReaderPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
    OFX::ContextEnum context)
{
    // there has to be an input clip, even for generators
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);

    srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setOptional(true);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGB);
    dstClip->setSupportsTiles(kSupportsTiles);

    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");

    generatorDescribeInContext(page, desc, *dstClip, eGeneratorExtentDefault, OFX::ePixelComponentRGBA, true, context);
    {
        OFX::StringParamDescriptor *param = desc.defineStringParam(kMLVfileParamter);
        param->setLabel("Filename");
        param->setHint("Name of the MLV file");
        param->setDefault("");
        param->setFilePathExists(true);
        param->setStringType(OFX::eStringTypeFilePath);
        if (page)
        {
            page->addChild(*param);
        }
        desc.addClipPreferencesSlaveParam(*param);
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
