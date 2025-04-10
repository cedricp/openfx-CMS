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
#include <gfx.h>

#include "CMSVectorScope.h"

OFXS_NAMESPACE_ANONYMOUS_ENTER

inline void RGB_BT709_2_YUV(const float rgb[3], float out[3])
{
    out[0] = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
    out[1] = -0.09991f * rgb[0] - 0.33609f * rgb[1] + 0.436f * rgb[2];
    out[2] = 0.615f * rgb[0] - 0.55861f * rgb[1] - 0.05639f * rgb[2];
}

inline uint16_t color16(char r, char g, char b)
{
    uint16_t color;
    color = (r >> 3) << 11;
    color |= (g >> 2) << 5;
    color |= (b >> 3);
    return color;
}

bool CMSVectorScope::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!kSupportsRenderScale && ((args.renderScale.x != 1.) || (args.renderScale.y != 1.)))
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);

        return false;
    }

    if (!_inputClip->isConnected()){
        return false;
    }

    OfxRectI window;
    _inputClip->getFormat(window);
    OfxRectD srod = _inputClip->getRegionOfDefinition(args.time);

    rod.x1 = 0;
    rod.x2 = srod.x2;
    rod.y1 = 0;
    rod.y2 = srod.y2;

    return true;
}

// the overridden render function
void CMSVectorScope::render(const OFX::RenderArguments &args)
{
    if (!_inputClip->isConnected()){
        return;
    }
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

    OfxRectI window = args.renderWindow;
    int scope_buffer_size = _scopeResolution.x * _scopeResolution.x * 3;
    std::vector<float> scope_buffer;
    scope_buffer.resize(scope_buffer_size);
    create_scope(src.get(), scope_buffer.data());
    OFX::Image *_dstimg = dst.get();
    OFX::Image *_srcimg = src.get();

    int _nComponentsDst = _outputClip->getPixelComponentCount();
    int _nComponentsSrc = _inputClip->getPixelComponentCount();

    for (int y = window.y1; y < window.y2; ++y){
        float *dstPix = (float *)_dstimg->getPixelAddress(window.x1, y);
        const float *srcPix = (float *)_srcimg->getPixelAddress(window.x1, y);
        if (!dstPix || !srcPix) continue;
        for (int x = window.x1; x < window.x2; ++x){
            int scopeCoordX = x - 30;
            int scopeCoordY = y - 30;

            if (scopeCoordX >= 0 && scopeCoordX < _scopeResolution.x && scopeCoordY >= 0 && scopeCoordY < _scopeResolution.y){
                // Borders of the scope are gray
                if (scopeCoordX == 0 || scopeCoordY == 0 || scopeCoordX == _scopeResolution.x - 1 || scopeCoordY == _scopeResolution.y - 1){
                    dstPix[0] = .6f;
                    dstPix[1] = .6f;
                    dstPix[2] = .6f;
                    if (_nComponentsDst > 3){
                        dstPix[3] = 1.f;
                    }
                } else {
                    float *scopePix = scope_buffer.data() + ((y - 30) * _scopeResolution.x + (x - 30)) * 3;
                    for (int i = 0; i < 3; ++i){
                        dstPix[i] = scopePix[i];
                    }
                    if (_nComponentsDst > 3) dstPix[3] = 1.f;
                }
            } else {
                for (int i = 0; i < 3; ++i){
                    dstPix[i] = srcPix[i];
                }
                if (_nComponentsDst > 3) dstPix[3] = _nComponentsSrc > 3 ? srcPix[3] : 1.f;
            }
            srcPix += _nComponentsSrc;
            dstPix += _nComponentsDst;
        }   
    }
}

void CMSVectorScope::create_scope(OFX::Image *input, float* buffer)
{
    GFXcanvasFloat scopeCanvas(_scopeResolution.x, _scopeResolution.y, buffer);
    int centerx = scopeCanvas.width() / 2;
    int centery = scopeCanvas.height() / 2;

    int circle_size = scopeCanvas.height() / 2.5;

    scopeCanvas.fillCircle(centerx, centery, circle_size, color16(50, 50, 50));

    scopeCanvas.drawLine( centerx, centery + circle_size,
        centerx, centery - circle_size, color16(100,100,100));

    scopeCanvas.drawLine( centerx + circle_size, centery,
        centerx - circle_size,centery, color16(100,100,100));

    int input_width = input->getBounds().x2;
    int input_height = input->getBounds().y2;
    int input_components = input->getPixelComponentCount();
    float yuv[3];

    for(int y = 0; y < input_height; ++y){
        const float *srcPix = (float *)input->getPixelAddress(0, y);
        if (!srcPix) continue;
        for(int x = 0; x < input_width; ++x){
            RGB_BT709_2_YUV(srcPix, yuv);
            int vx = centerx + (yuv[1]*255);
            int vy = centery + (yuv[2]*255);
            if (vx > 0 && vx < _scopeResolution.x && vy > 0 && vy < _scopeResolution.y){
                float* destPix = buffer + (vy * _scopeResolution.x + vx) * 3;
                if (destPix[0] < 1 && destPix[1] < 1 && destPix[2] < 1){
                    destPix[0] += srcPix[0] * .1f;
                    destPix[1] += srcPix[1] * .1f;
                    destPix[2] += srcPix[2] * .1f;
                }
            }
            srcPix += input_components;
        }
    }

    float red[3] = {1,0,0};
    float green[3] = {0,1,0};
    float blue[3] = {0,0,1};
    float yellow[3] = {1,1,0};
    float magenta[3] = {1,0,1};
    float cyan[3] = {0,1,1};
    float fleshtone[3] = {1., 0.7731, 0.67};
    vec2f pos;

    for (int i = 0; i < 360; i+=10){
        float size = i % 20 ? 6 : 12;
        float cs = cos(i * M_PI / 180.);
        float sn = sin(i * M_PI / 180.);
        float x1 = centerx + cs * circle_size;
        float y1 = centery + sn * circle_size;
        float x2 = centerx + cs * (circle_size - size);
        float y2 = centery + sn * (circle_size - size);
        scopeCanvas.drawLine(x1, y1, x2, y2, color16(70,70,70));
    }

    // Draw skin tone
    RGB_BT709_2_YUV(fleshtone, yuv);
    vec2f uv(yuv[1], -yuv[2]);
    float norm = uv.norm();
    uv /= norm;
    uv *= circle_size;
    pos.x = centerx + uv.x;
    pos.y = centery + uv.y;
    scopeCanvas.drawLine(centerx, centery, pos.x, pos.y, color16(70,70,70));

    // Draw color components
    RGB_BT709_2_YUV(red, yuv);
    pos.x = 250 + (yuv[1]*255);
    pos.y = 250 + (-yuv[2]*255);
    scopeCanvas.drawRect(pos.x, pos.y, 10, 10, color16(100,100,100));
    scopeCanvas.drawChar(pos.x, pos.y - 6, 'R', color16(100,0,0), color16(100,100,100), 1);

    RGB_BT709_2_YUV(green, yuv);
    pos.x = 250 + (yuv[1]*255);
    pos.y = 250 + (-yuv[2]*255);
    scopeCanvas.drawRect(pos.x, pos.y, 10, 10, color16(100,100,100));
    scopeCanvas.drawChar(pos.x, pos.y - 6, 'G', color16(0,100,0), color16(100,100,0), 1);

    RGB_BT709_2_YUV(blue, yuv);
    pos.x = 250 + (yuv[1]*255);
    pos.y = 250 + (-yuv[2]*255);
    scopeCanvas.drawRect(pos.x, pos.y, 10, 10, color16(100,100,100));
    scopeCanvas.drawChar(pos.x, pos.y - 6, 'B', color16(0,0,100), color16(160,100,100), 1);

    RGB_BT709_2_YUV(yellow, yuv);
    pos.x = 250 + (yuv[1]*255);
    pos.y = 250 + (-yuv[2]*255);
    scopeCanvas.drawRect(pos.x, pos.y, 10, 10, color16(100,100,100));
    scopeCanvas.drawChar(pos.x, pos.y - 6, 'Y', color16(100,100,0), color16(100,100,100), 1);
    
    RGB_BT709_2_YUV(cyan, yuv);
    pos.x = 250 + (yuv[1]*255);
    pos.y = 250 + (-yuv[2]*255);
    scopeCanvas.drawRect(pos.x, pos.y, 10, 10, color16(100,100,100));
    scopeCanvas.drawChar(pos.x, pos.y - 6, 'C', color16(0,100,100), color16(100,100,100), 1);

    RGB_BT709_2_YUV(magenta, yuv);
    pos.x = 250 + (yuv[1]*255);
    pos.y = 250 + (-yuv[2]*255);
    scopeCanvas.drawRect(pos.x, pos.y, 10, 10, color16(100,100,100));
    scopeCanvas.drawChar(pos.x, pos.y - 6, 'M', color16(100,0,100), color16(100,100,100), 1);
}


void CMSVectorScope::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    if (!_inputClip->isConnected()){
        return;
    }
    OfxRectI format;
    _inputClip->getFormat(format);
    clipPreferences.setOutputFormat(format);
    clipPreferences.setPixelAspectRatio(*_outputClip, 1);
    clipPreferences.setClipBitDepth(*_outputClip, OFX::eBitDepthFloat);
    clipPreferences.setClipComponents(*_outputClip, OFX::ePixelComponentRGBA);
    clipPreferences.setOutputPremultiplication(OFX::eImageOpaque);


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
}

bool
CMSVectorScope::isIdentity(const OFX::IsIdentityArguments &args,
                             OFX::Clip * &identityClip,
                             double & /*identityTime*/
                             , int& /*view*/, std::string& /*plane*/)
{
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
