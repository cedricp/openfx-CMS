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

#include "CMSBakeLut.h"

OFXS_NAMESPACE_ANONYMOUS_ENTER

bool CMSBakeLutPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!kSupportsRenderScale && ((args.renderScale.x != 1.) || (args.renderScale.y != 1.)))
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);

        return false;
    }
    getOutputRoD(args.time, args.view, &rod, 0);

    return true;
}

void CMSBakeLutPlugin::getOutputRoD(OfxTime time, int view, OfxRectD *rod, double *par)
{
    assert(rod || par);

    // user wants RoD written, don't care parameters

    if (rod)
    {
        *rod = _inputClip->getRegionOfDefinition(time, view);
    }
    if (par)
    {
        *par = _inputClip->getPixelAspectRatio();
    }
}

// the overridden render function
void CMSBakeLutPlugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    const double time = args.time;
    OFX::BitDepthEnum dstBitDepth = _dstClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _dstClip->getPixelComponents();

    assert(OFX_COMPONENTS_OK(dstComponents));

    checkComponents(dstBitDepth, dstComponents);

    
    OFX::auto_ptr<OFX::Image> dst(_dstClip->fetchImage(time));
    std::unique_ptr<OFX::Image> src(_inputClip->fetchImage(args.time));
    if (!src.get())
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    OfxRectD rod = _inputClip->getRegionOfDefinition(time, args.renderView);
    printf(">> %f %f %f %f\n", rod.x1, rod.y1, rod.x2, rod.y2);
    

    int width_img = (int)(rod.x2 - rod.x1);
    int height_img = (int)(rod.y2 - rod.y1);

    float num_x = width_img / 7;
    float num_y = height_img / 7;
    int num_samples = num_x * num_y;
    int lut_size = round(pow(num_samples, 1.0 / 3.0));
    int total_samples = pow(lut_size, 3);
    int samples_count = 0;

    _lut.resize(total_samples);
    _lut.clear();

    for(int y=0; y < num_y; y++) {
        for(int x=0; x < num_x; x++) {
            float *srcPix = (float *)src->getPixelAddress(x*7 + 3, y*7 + 3);
            float *dstPix = (float *)dst->getPixelAddress(x, y);
            memcpy(dstPix, srcPix, 3 * sizeof(float));
            Color sample;
            sample.r = srcPix[0];
            sample.g = srcPix[1];
            sample.b = srcPix[2];
            _lut.push_back(sample);

            samples_count++;
            if (samples_count >= total_samples) {
                break;
            }
        }
    }

}

void CMSBakeLutPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    // output is continuous
    clipPreferences.setOutputHasContinuousSamples(true);

    GeneratorPlugin::getClipPreferences(clipPreferences);
}

void CMSBakeLutPlugin::changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName)
{
    if (paramName == "BakeLUT")
    {
        printf("BakeLUT !!\n");
    }
}

void CMSBakeLutPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    desc.setLabel(kPluginName);
    desc.setPluginDescription(kPluginDescription);
    desc.setPluginGrouping(kPluginGrouping);
    desc.addSupportedContext(OFX::eContextGenerator);
    desc.addSupportedContext(OFX::eContextGeneral);
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
#ifdef OFX_EXTENSIONS_NATRON
    desc.setChannelSelector(OFX::ePixelComponentRGB);
#endif

    OFX::generatorDescribe(desc);
}


void CMSBakeLutPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
                                                OFX::ContextEnum context)
{
    // there has to be an input clip, even for generators
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);

    srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setOptional(false);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGB);
    dstClip->setSupportsTiles(kSupportsTiles);

    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");

    generatorDescribeInContext(page, desc, *dstClip, eGeneratorExtentDefault, OFX::ePixelComponentRGBA, true, context);

    {
        {
            OFX::StringParamDescriptor *param = desc.defineStringParam(kOfxImageEffectFileParamName);
            param->setStringType(OFX::eStringTypeFilePath);
            param->setLabel("output LUT file");
            param->setAnimates(false);
            param->setHint("The file where the LUT will be saved.");
            param->setDefault("test.cube");
            desc.addClipPreferencesSlaveParam(*param);
            if (page)
                page->addChild(*param);
        }

        {
            OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam("BakeLUT");
            param->setLabel("Bake LUT");
            param->setHint("Create the 3D LUT file in cube format.");
            if (page) {
                page->addChild(*param);
            }
        }
    }
}

OFX::ImageEffect *
CMSBakeLutPluginFactory::createInstance(OfxImageEffectHandle handle,
                                        OFX::ContextEnum /*context*/)
{
    return new CMSBakeLutPlugin(handle);
}

static CMSBakeLutPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

    OFXS_NAMESPACE_ANONYMOUS_EXIT
