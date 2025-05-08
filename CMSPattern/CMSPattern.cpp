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
#include "CMSPattern.h"

OFXS_NAMESPACE_ANONYMOUS_ENTER

class CMSPatternProcessor
    : public OFX::ImageProcessor
{
public:
    CMSPatternProcessor(OFX::ImageEffect &instance): ImageProcessor(instance)
    {
        _res.x  =_res.y = 0;
    } 

    ~CMSPatternProcessor()
    {
    }

    void setValues(const int lutsize,
                    const OfxPointI &rod)
    {
        _lutsize = lutsize;
        _res = rod;
    }

private:
    void multiThreadProcessImages(const OfxRectI &procWindow, const OfxPointD &rs) OVERRIDE FINAL
    {
        OFX::unused(rs);
        int lut_sqr = _lutsize * _lutsize;
        int lut_cub = lut_sqr * _lutsize;
        int num_x = _res.x / 7;

        bool alpha = _dstImg->getPixelComponents() == OFX::ePixelComponentRGBA;

        for (int y = procWindow.y1; y < procWindow.y2; y++)
        {
            if (_effect.abort())
            {
                break;
            }
            float *dstPix = static_cast<float*>(_dstImg->getPixelAddress(procWindow.x1, y));
            if (y > _res.y){
                *dstPix++ = 0;
                *dstPix++ = 0;
                *dstPix++ = 0;
                if (alpha) *dstPix++ = 0;
            }
            int curr_y = y / 7 * num_x;
            for (int x = procWindow.x1; x < procWindow.x2; ++x)
            {
                if (x > _res.x){
                    *dstPix++ = 0;
                    *dstPix++ = 0;
                    *dstPix++ = 0;
                    if (alpha) *dstPix++ = 0;
                    continue;
                } 
                int curr_x = x / 7;
                int curr_pos = curr_y + curr_x;
                if (curr_pos >= lut_cub)
                {
                    *dstPix++ = 0;
                    *dstPix++ = 0;
                    *dstPix++ = 0;
                    if (alpha) *dstPix++ = 0;
                } else {
                    float curr_red = float(curr_pos % _lutsize) / (_lutsize - 1);
                    float curr_green = float((curr_pos / _lutsize) % _lutsize) / (_lutsize - 1);
                    float curr_blue = float((curr_pos / _lutsize / _lutsize) % _lutsize) / (_lutsize - 1);
                    *dstPix++ = curr_red;
                    *dstPix++ = curr_green;  
                    *dstPix++ = curr_blue;
                    if (alpha) *dstPix++ = 1;
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

void CMSPatternPlugin::changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName)
{
    if (paramName == kParamLUTSizeName)
    {

    }
}


// the overridden render function
void CMSPatternPlugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    const double time = args.time;
    OFX::BitDepthEnum dstBitDepth = _dstClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _dstClip->getPixelComponents();

    assert(OFX_COMPONENTS_OK(dstComponents));

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
    processor.setValues(lutsize, resolution);
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

bool CMSPatternPlugin::getTimeDomain(OfxRangeD& range)
{
    range.min = 0;
    range.max = 1;
    return true;
}

bool CMSPatternPlugin::isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& identityClip, double& identityTime, int& view, std::string& plane)
{
    return false;
}

void CMSPatternPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    OfxPointI res = getCMSResolution();
    OfxRectI format;
    format.x1 = 0;
    format.x2 = res.x;
    format.y1 = 0;
    format.y2 = res.y;

    clipPreferences.setOutputFrameVarying(false);
    clipPreferences.setOutputFormat(format);

    clipPreferences.setPixelAspectRatio(*_dstClip, 1);
    clipPreferences.setClipBitDepth(*_dstClip, OFX::eBitDepthFloat);
    clipPreferences.setClipComponents(*_dstClip, OFX::ePixelComponentRGBA);

    clipPreferences.setOutputHasContinuousSamples(true);
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
    desc.setHostFrameThreading(true);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderTwiceAlways(false);
    desc.setRenderThreadSafety(OFX::kRenderThreadSafety);
}

void CMSPatternPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
    OFX::ContextEnum context)
{
    // there has to be an input clip, even for generators
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setOptional(true);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGB);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA); // For some reason, Nuke crashes if not enabled
    dstClip->setSupportsTiles(kSupportsTiles);

    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");

    {
        OFX::IntParamDescriptor *param = desc.defineIntParam(kParamLUTSizeName);
        param->setLabel(kParamLutSizeLabel);
        param->setHint(kParamLutSizeHint);
        param->setDefault(kParamLUTSize);
        param->setRange(8, 33);
        param->setDisplayRange(8, 33);
        desc.addClipPreferencesSlaveParam(*param);
        if (page)
        {
            page->addChild(*param);
        }
    }

    {
        OFX::BooleanParamDescriptor * param = desc.defineBooleanParam(kParamLog2EncodeEnable);
        param->setLabel("AntiLog2 Encode");
        param->setHint("AntiLog2 encode output samples");
        param->setDefault(false);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        OFX::Double2DParamDescriptor * param = desc.defineDouble2DParam(kParamLog2MinMax);
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

void loadPlugin()
{
    OFX::ofxsThreadSuiteCheck();
}

static CMSPatternPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
static OFX::PluginFactories mypluginfactory_p(&p);

OFXS_NAMESPACE_ANONYMOUS_EXIT
