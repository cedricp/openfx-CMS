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
#include "../utils/utils.h"

OFXS_NAMESPACE_ANONYMOUS_ENTER


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

bool CMSBakeLutPlugin::isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& identityClip, double& identityTime, int& view, std::string& plane)
{
    return false;
}
// the overridden render function
void CMSBakeLutPlugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    const double time = args.time;
    OFX::BitDepthEnum dstBitDepth = _dstClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _dstClip->getPixelComponents();

    assert(OFX_COMPONENTS_OK(dstComponents));

    //checkComponents(dstBitDepth, dstComponents);
    
    OFX::auto_ptr<OFX::Image> dst(_dstClip->fetchImage(time));
    OFX::auto_ptr<OFX::Image> src(_inputClip->fetchImage(args.time));
    if (!src.get())
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }
    OfxRectD rodd = _dstClip->getRegionOfDefinition(time, args.renderView);
    OfxRectD rods = _inputClip->getRegionOfDefinition(time, args.renderView);


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
    if (!_inputClip->isConnected()){
        return;
    }
    OfxRectI format;
    _inputClip->getFormat(format);

    clipPreferences.setOutputFormat(format);
    
    clipPreferences.setClipBitDepth(*_inputClip, OFX::eBitDepthFloat);
    clipPreferences.setClipComponents(*_inputClip, OFX::ePixelComponentRGBA);
    
    clipPreferences.setPixelAspectRatio(*_dstClip, 1);
    clipPreferences.setClipBitDepth(*_dstClip, OFX::eBitDepthFloat);
    clipPreferences.setClipComponents(*_dstClip, OFX::ePixelComponentRGBA);

    clipPreferences.setOutputHasContinuousSamples(true);
}

void CMSBakeLutPlugin::changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName)
{
    if (paramName == "BakeLUT")
    {
        std::string filename = _outputLutFile->getValue();
        FILE *file = fopen(filename.c_str(), "w");
        // const int lut1dsize_choice = _lut1dsize->getValue();
        // int lut1dsize = 512;
        // if (lut1dsize_choice == 1) lut1dsize = 1024;
        // if (lut1dsize_choice == 2) lut1dsize = 2048;
        // if (lut1dsize_choice == 3) lut1dsize = 4096;

        if (file == NULL) {
            printf("Error opening file %s\n", filename.c_str());
            return;
        }

        fprintf(file, "LUT_3D_SIZE %d\n", _lutSize);
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
    desc.setRenderThreadSafety(OFX::eRenderInstanceSafe);
#ifdef OFX_EXTENSIONS_NATRON
    desc.setChannelSelector(OFX::ePixelComponentRGBA);
#endif

}


void CMSBakeLutPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
                                                OFX::ContextEnum context)
{
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setOptional(false);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGB);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    dstClip->setSupportsTiles(kSupportsTiles);

    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");

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
