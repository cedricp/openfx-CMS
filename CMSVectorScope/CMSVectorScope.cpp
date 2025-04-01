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
#include <atomic>

#include "CMSVectorScope.h"

OFXS_NAMESPACE_ANONYMOUS_ENTER

inline void RGB_BT709_2_YUV(const float rgb[3], float out[3])
{
    out[0] = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
    out[1] = -0.09991f * rgb[0] - 0.33609f * rgb[1] + 0.436f * rgb[2];
    out[2] = 0.615f * rgb[0] - 0.55861f * rgb[1] - 0.05639f * rgb[2];
}

class VectorScopeProcessor : public OFX::ImageProcessor
{
public:
    VectorScopeProcessor(OFX::ImageEffect &instance) : ImageProcessor(instance)
    {

    }

    ~VectorScopeProcessor()
    {
    }

    void setValues(OFX::Image *src, OFX::Image *dst)
    {
        _srcimg = src;
        _dstimg = dst;
        _nComponentsSrc = _srcimg->getPixelComponentCount();
        _nComponentsDst = _dstimg->getPixelComponentCount();
    }

    OFX::Image *_srcimg, *_dstimg;
private:
    int _nComponentsSrc;
    int _nComponentsDst;

    void multiThreadProcessImages(const OfxRectI &procWindow, const OfxPointD &rs) OVERRIDE FINAL
    {
        OFX::unused(rs);

        int alphaDst = _nComponentsDst == 4;

        for (int y = procWindow.y1; y < procWindow.y2; ++y){
            const float *srcPix = (float *)_srcimg->getPixelAddress(procWindow.x1, y);
            if (!srcPix) continue;
            for (int x = procWindow.x1; x < procWindow.x2; ++x){
                float yuv[3];
                RGB_BT709_2_YUV(srcPix, yuv);
                int vx = 255. + (yuv[1]*127.);
                int vy = 255. + -(yuv[2]*127.);

                float *dstPix = (float *)_dstimg->getPixelAddress(vx, vy);
                if (!dstPix) continue;
                // std::atomic<float> Rin(dstPix[0]);
                // float Rout = Rin.load() + .1;
                // Rin.store(Rout);
                // std::atomic<float> Gin(dstPix[1]);
                // float Gout = Gin.load() + .1;
                // Rin.store(Gout);
                // std::atomic<float> Bin(dstPix[2]);
                // float Bout = Bin.load() + .1;
                // Rin.store(Bout);
                dstPix[0] += srcPix[0] * .1f;
                dstPix[1] += srcPix[1] * .1f;
                dstPix[2] += srcPix[2] * .1f;

                srcPix += _nComponentsSrc;
            }
        }
    }
};

bool CMSVectorScope::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!kSupportsRenderScale && ((args.renderScale.x != 1.) || (args.renderScale.y != 1.)))
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);

        return false;
    }

    if (_inputClip->isConnected()){
        return false;
    }

    // OfxRectI window;
    // _inputClip->getFormat(window);

    rod.x1 = 0;
    rod.x2 = 1920;
    rod.y1 = 0;
    rod.y2 = 1080;

    return true;
}

// the overridden render function
void CMSVectorScope::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    const double time = args.time;
    OFX::BitDepthEnum dstBitDepth = _outputClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = _outputClip->getPixelComponents();

    assert(OFX_COMPONENTS_OK(dstComponents));

    OFX::auto_ptr<OFX::Image> dst(_outputClip->fetchImage(time));
    OFX::auto_ptr<OFX::Image> src(_inputClip->fetchImage(time));
    if (!src.get() || !dst.get())
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }

    OfxRectI window;
    _inputClip->getFormat(window);

    // VectorScopeProcessor processor(*this);
    // processor.setRenderWindow(window, args.renderScale);
    // processor.setValues(src.get(), dst.get());
    // processor.process();
    OFX::Image *_srcimg = src.get();
    OFX::Image *_dstimg = dst.get();

    int _nComponentsSrc = _inputClip->getPixelComponentCount();

    for (int y = window.y1; y < window.y2; ++y){
        const float *srcPix = (float *)_srcimg->getPixelAddress(window.x1, y);
        if (!srcPix) continue;
        for (int x = window.x1; x < window.x2; ++x){
            float yuv[3];
            RGB_BT709_2_YUV(srcPix, yuv);
            int vx = 255. + (yuv[1]*128);
            int vy = 255. + -(yuv[2]*128);

            float *dstPix = (float *)_dstimg->getPixelAddress(vx, vy);
            if (!dstPix) continue;
            dstPix[0] += srcPix[0] * .1f;
            dstPix[1] += srcPix[1] * .1f;
            dstPix[2] += srcPix[2] * .1f;
            dstPix[3] = 1.f;

            srcPix += _nComponentsSrc;
        }
    }
}


void CMSVectorScope::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    if (!_inputClip->isConnected()){
        return;
    }
    OfxRectI format;
    //_inputClip->getFormat(format);
    format.x2 = format.y2 = 512;
    format.x1 = format.y1 = 0;
    clipPreferences.setOutputFormat(format);
    clipPreferences.setPixelAspectRatio(*_outputClip, 1);
    clipPreferences.setClipBitDepth(*_outputClip, OFX::eBitDepthFloat);
    clipPreferences.setClipComponents(*_outputClip, OFX::ePixelComponentRGBA);

    // output is continuous
    clipPreferences.setOutputHasContinuousSamples(true);
}

void CMSVectorScope::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{

}

void CMSVectorScopeFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    desc.setLabel(kPluginName);
    desc.setPluginDescription(kPluginDescription);
    desc.setPluginGrouping(kPluginGrouping);
    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(true);
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
CMSVectorScope::isIdentity(const OFX::IsIdentityArguments &args,
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
CMSVectorScopeFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
                                                    OFX::ContextEnum context)
{
    // there has to be an input clip, even for generators
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
            // OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kParamAntilog);
            // param->setLabel("antiLog");
            // param->setHint("Log/Antilog switch");
            // param->setDefault(false);
            // if (page)
            // {
            //     page->addChild(*param);
            // }
        }
    }
}

OFX::ImageEffect *
CMSVectorScopeFactory::createInstance(OfxImageEffectHandle handle,
                                            OFX::ContextEnum /*context*/)
{
    return new CMSVectorScope(handle);
}

static CMSVectorScopeFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

    OFXS_NAMESPACE_ANONYMOUS_EXIT
