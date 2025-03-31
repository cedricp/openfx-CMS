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

#include "CMSLogEncoding.h"
#include "../utils.h"

OFXS_NAMESPACE_ANONYMOUS_ENTER

class CMSLogProcessor
    : public OFX::ImageProcessor
{
public:
    CMSLogProcessor(OFX::ImageEffect &instance) : ImageProcessor(instance)
    {
        _min = -8;
        _max = 4;
        _logencoder = nullptr;
    }

    ~CMSLogProcessor()
    {
        if (_logencoder)
        {
            delete _logencoder;
        }
    }

    void setValues(const bool isAntilog,
                   double logmin, double logmax,
                   OFX::Image *src)
    {
        if (_logencoder)
            delete _logencoder;

        _antiLog = isAntilog;
        _min = logmin;
        _max = logmax;
        _src = src;
        _logencoder = new logEncode(logmin, logmax);
        _nComponentsSrc = _src->getPixelComponentCount();
        _nComponentsDst = _dstImg->getPixelComponentCount();
    }

private:
    logEncode *_logencoder;
    bool _antiLog;
    double _min, _max;
    OFX::Image *_src;
    int _nComponentsSrc;
    int _nComponentsDst;

    void multiThreadProcessImages(const OfxRectI &procWindow, const OfxPointD &rs) OVERRIDE FINAL
    {
        OFX::unused(rs);

        int alphaDst = _nComponentsDst == 4;
        int alphaSrc = _nComponentsSrc == 4;

        if (_logencoder == nullptr)
        {
            return;
        }

        for (int y = procWindow.y1; y < procWindow.y2; y++)
        {
            if (_effect.abort())
            {
                break;
            }

            float *dstPix = (float *)_dstImg->getPixelAddress(procWindow.x1, y);
            const float *srcPix = (float *)_src->getPixelAddress(procWindow.x1, y);
            if (!dstPix || !srcPix)
            {
                continue;
            }

            for (int x = procWindow.x1; x < procWindow.x2; ++x)
            {
                for (int z = 0; z < 3; ++z)
                {
                    if (!_antiLog)
                    {
                        *dstPix++ = _logencoder->apply(*srcPix++);
                    }
                    else
                    {
                        *dstPix++ = _logencoder->apply_backward(*srcPix++);
                    }
                }
                if (alphaDst && alphaSrc)
                {
                    *dstPix++ = *srcPix++;
                }
                else if (alphaDst)
                {
                    *dstPix++ = 1.0;
                }
                else if (alphaSrc)
                {
                    *srcPix++;
                }
            }
        }
    }
};

bool CMSLogEncodingPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
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
void CMSLogEncodingPlugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    const double time = args.time;
    OFX::BitDepthEnum dstBitDepth = _outputClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _outputClip->getPixelComponents();

    assert(OFX_COMPONENTS_OK(dstComponents));

    //checkComponents(dstBitDepth, dstComponents);

    OFX::auto_ptr<OFX::Image> dst(_outputClip->fetchImage(time));
    OFX::auto_ptr<OFX::Image> src(_inputClip->fetchImage(args.time));
    if (!src.get())
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }
    OfxRectD rodd = _outputClip->getRegionOfDefinition(time, args.renderView);
    OfxRectD rods = _inputClip->getRegionOfDefinition(time, args.renderView);

    bool isAntiLog = _isAntiLog->getValue();
    double logmin, logmax;
    _logminmax->getValue(logmin, logmax);

    CMSLogProcessor processor(*this);
    processor.setDstImg(dst.get());
    processor.setRenderWindow(args.renderWindow, args.renderScale);
    processor.setValues(isAntiLog, logmin, logmax, src.get());
    processor.process();
}

void CMSLogEncodingPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    if (!_inputClip->isConnected()){
        return;
    }
    OfxRectI format;
    _inputClip->getFormat(format);
    double par = 1.;
    clipPreferences.setOutputFormat(format);
    clipPreferences.setPixelAspectRatio(*_outputClip, par);
    clipPreferences.setClipBitDepth(*_outputClip, OFX::eBitDepthFloat);
    clipPreferences.setClipComponents(*_outputClip, OFX::ePixelComponentRGB);

    // output is continuous
    clipPreferences.setOutputHasContinuousSamples(true);
}

void CMSLogEncodingPlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{

}

void CMSLogEncodingPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    desc.setLabel(kPluginName);
    desc.setPluginDescription(kPluginDescription);
    desc.setPluginGrouping(kPluginGrouping);
    desc.addSupportedContext(OFX::eContextPaint);
    desc.addSupportedContext(OFX::eContextGeneral);
    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderThreadSafety(OFX::kRenderThreadSafety);
#ifdef OFX_EXTENSIONS_NATRON
    desc.setChannelSelector(OFX::ePixelComponentRGB);
#endif

}

bool
CMSLogEncodingPlugin::isIdentity(const OFX::IsIdentityArguments &args,
                             OFX::Clip * &identityClip,
                             double & /*identityTime*/
                             , int& /*view*/, std::string& /*plane*/)
{
    if (0){
        identityClip = _inputClip;
        return true;
    }
    return false;
}

void 
CMSLogEncodingPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
                                                    OFX::ContextEnum context)
{
    // there has to be an input clip, even for generators
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);

    srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setOptional(false);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGB);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    dstClip->setSupportsTiles(kSupportsTiles);

    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");

    {
        {
            OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kParamAntilog);
            param->setLabel("antiLog");
            param->setHint("Log/Antilog switch");
            param->setDefault(false);
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::Double2DParamDescriptor *param = desc.defineDouble2DParam(kParamMinMax);
            param->setLabel("Log2 Min Max values");
            param->setHint("Min and max exposure values");
            param->setDefault(-8, 4);
            if (page)
            {
                page->addChild(*param);
            }
        }
    }
}

OFX::ImageEffect *
CMSLogEncodingPluginFactory::createInstance(OfxImageEffectHandle handle,
                                            OFX::ContextEnum /*context*/)
{
    return new CMSLogEncodingPlugin(handle);
}

static CMSLogEncodingPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

    OFXS_NAMESPACE_ANONYMOUS_EXIT
