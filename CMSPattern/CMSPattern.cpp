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

#include "CMSPattern.h"
#include "../utils.h" 


OFXS_NAMESPACE_ANONYMOUS_ENTER

class CMSPatternProcessor
    : public OFX::ImageProcessor
{
public:
    CMSPatternProcessor(OFX::ImageEffect &instance): ImageProcessor(instance)
    {
        _logencoder = nullptr;
        _res.x  =_res.y = 0;
    } 

    ~CMSPatternProcessor()
    {
        if (_logencoder) delete _logencoder;
    }

    void setValues(const int lutsize,
                    const OfxPointI &rod, bool isAntiLog,
                    double logmin, double logmax)
    {
        if (_logencoder) delete _logencoder;
        _lutsize = lutsize;
        _res = rod;
        if (isAntiLog){
            _logencoder = new logEncode(logmin, logmax);
        } else {
            _logencoder = nullptr;
        }
    }

private:
    logEncode* _logencoder;
    void multiThreadProcessImages(const OfxRectI &procWindow, const OfxPointD &rs) OVERRIDE FINAL
    {
        OFX::unused(rs);
        int lut_sqr = _lutsize * _lutsize;
        int lut_cub = lut_sqr * _lutsize;
        int num_x = _res.x / 7;

        for (int y = procWindow.y1; y < procWindow.y2; y++)
        {
            if (_effect.abort())
            {
                break;
            } 
                
            float *dstPix = (float *)_dstImg->getPixelAddress(procWindow.x1, y);
            int curr_y = y / 7 * num_x;
            for (int x = procWindow.x1; x < procWindow.x2; ++x)
            {
                int curr_x = x / 7;
                int curr_pos = curr_y + curr_x;
                if (curr_pos >= lut_cub)
                {
                    *dstPix++ = 0;
                    *dstPix++ = 0;
                    *dstPix++ = 0;
                } else {
                    float curr_red = float(curr_pos % _lutsize) / (_lutsize - 1);
                    float curr_green = float((curr_pos / _lutsize) % _lutsize) / (_lutsize - 1);
                    float curr_blue = float((curr_pos / _lutsize / _lutsize) % _lutsize) / (_lutsize - 1);
                    if (_logencoder){
                        curr_red = _logencoder->apply_backward(curr_red);
                        curr_green = _logencoder->apply_backward(curr_green);
                        curr_blue = _logencoder->apply_backward(curr_blue);
                    }
                    *dstPix++ = curr_red;
                    *dstPix++ = curr_green;  
                    *dstPix++ = curr_blue;
                }
            }
        }
    }

    int _lutsize;
    OfxPointI _res;
};


bool CMSPatternPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!kSupportsRenderScale && ((args.renderScale.x != 1.) || (args.renderScale.y != 1.))) {
        OFX::throwSuiteStatusException(kOfxStatFailed);

        return false;
    }
    
    auto resolution = getCMSResolution();
    rod.x1 = 0;
    rod.x2 = resolution.x;
    rod.y1 = 0;
    rod.y2 = resolution.y;
    return true;
}


// the overridden render function
void CMSPatternPlugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    const double time = args.time;
    OFX::BitDepthEnum dstBitDepth = _dstClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _dstClip->getPixelComponents();

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

    CMSPatternProcessor processor(*this);
    processor.setDstImg(dst.get());
    processor.setRenderWindow(args.renderWindow, args.renderScale);

    double lutsize = _lutSize->getValue();
    OfxPointI resolution = getCMSResolution();
    bool isAntilog = _antiLogScale->getValue();
    double min, max;
    _logminmax->getValue(min, max);
    processor.setValues(lutsize, resolution, isAntilog, min, max);
    processor.process();
}

OfxPointI CMSPatternPlugin::getCMSResolution()
{
    OfxPointI res;
    float lut_samples = _lutSize->getValue();
    lut_samples *= lut_samples * lut_samples; // ^3
    float num = sqrt(lut_samples);
    res.y = floor(num);
    res.x = res.y;

    
    while (res.x * res.y < lut_samples)
    {
        res.y += 1;
    }

    res.x *= 7;
    res.y *= 7;
    return res;
}


void CMSPatternPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    // output is continuous
    clipPreferences.setOutputHasContinuousSamples(true);

    GeneratorPlugin::getClipPreferences(clipPreferences);
}

void CMSPatternPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
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
    desc.setChannelSelector(OFX::ePixelComponentRGBA);
#endif

    OFX::generatorDescribe(desc);
}

void CMSPatternPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
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
        OFX::IntParamDescriptor *param = desc.defineIntParam(kParamLUTSize);
        param->setLabel(kParamBarIntensityLabel);
        param->setHint(kParamBarIntensityHint);
        param->setDefault(kParamBarIntensityDefault);
        param->setRange(0., 33);
        param->setDisplayRange(0., 33);
        if (page)
        {
            page->addChild(*param);
        }
    }

    {
        OFX::BooleanParamDescriptor * param = desc.defineBooleanParam("log2 encode");
        param->setLabel("AntiLog2 Encode");
        param->setHint("AntiLog2 encode output samples");
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

OFX::ImageEffect *
CMSPatternPluginFactory::createInstance(OfxImageEffectHandle handle,
    OFX::ContextEnum /*context*/)
{
    return new CMSPatternPlugin(handle);
}

static CMSPatternPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

OFXS_NAMESPACE_ANONYMOUS_EXIT
