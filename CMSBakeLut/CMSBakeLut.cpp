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

#include "../utils.h"

bool CMSBakeLutPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!kSupportsRenderScale && ((args.renderScale.x != 1.) || (args.renderScale.y != 1.)))
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);

        return false;
    }

    rod = _inputClip->getRegionOfDefinition(args.time, args.view);

    return true;
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
    OFX::auto_ptr<OFX::Image> src(_inputClip->fetchImage(args.time));
    if (!src.get())
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }
    OfxRectD rodd = _dstClip->getRegionOfDefinition(time, args.renderView);
    OfxRectD rods = _inputClip->getRegionOfDefinition(time, args.renderView);

    bool isLog = _logScale->getValue();
    double logmin, logmax;
    _logminmax->getValue(logmin, logmax);
    logEncode encoder(logmin, logmax);


    int width_img = (int)(rods.x2 - rods.x1);
    int height_img = (int)(rods.y2 - rods.y1);

    float num_x = width_img / 7;
    float num_y = height_img / 7;
    int num_samples = num_x * num_y;
    _lutSize = round(pow(num_samples, 1.0 / 3.0));
    int total_samples = pow(_lutSize, 3);
    int sample_count = 0;

    _lut.resize(total_samples);

    for(int y=0; y < num_y; y++) {
        for(int x=0; x < num_x; x++) {
            float *srcPix = (float *)src->getPixelAddress((x+rods.x1)*7 + 3, (y+rods.y1)*7 + 3);
            float *dstPix = (float *)dst->getPixelAddress(x+rodd.x1, y+rodd.y1);
            Color sample;
            sample.r = srcPix[0];
            sample.g = srcPix[1];
            sample.b = srcPix[2];
            if (isLog){
                sample.r = encoder.apply(sample.r);
                sample.g = encoder.apply(sample.g);
                sample.b = encoder.apply(sample.b);
            }
            _lut[sample_count] = sample;
            dstPix[0] = sample.r;
            dstPix[1] = sample.g;
            dstPix[2] = sample.b;
            sample_count++;
            if (sample_count >= total_samples) {
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
        std::string filename = _outputLutFile->getValue();
        FILE *file = fopen(filename.c_str(), "w");
        if (file == NULL) {
            printf("Error opening file %s\n", filename.c_str());
            return;
        }
        fprintf(file, "# Generated LUT with openfx-CMS\n");
        fprintf(file, "LUT_3D_SIZE %i\n\n", _lutSize);
        fprintf(file, "DOMAIN_MIN 0.0 0.0 0.0\n");
        fprintf(file, "DOMAIN_MAX 1.0 1.0 1.0\n\n");
        for (int i = 0; i < _lut.size(); i++) {
            fprintf(file, "%f %f %f\n", _lut[i].r, _lut[i].g, _lut[i].b);
        }
        fclose(file);
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

        {
            OFX::BooleanParamDescriptor * param = desc.defineBooleanParam("log2 encode");
            param->setLabel("Log2 Encode");
            param->setHint("Log2 encode input samples");
            param->setDefault(false);
            if (page) {
                page->addChild(*param);
            }
        }

        {
            OFX::Double2DParamDescriptor * param = desc.defineDouble2DParam("log2 min max");
            param->setLabel("Log2 Min Max values");
            param->setHint("Min and max exposure values");
            param->setDefault(-8, 4);
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
