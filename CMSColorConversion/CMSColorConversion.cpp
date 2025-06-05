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

#include "CMSColorConversion.h"
#include "../utils/utils.h"
#include "../utils/mathutils.h"

OFXS_NAMESPACE_ANONYMOUS_ENTER

class CMSConversionProcessor
    : public OFX::ImageProcessor
{
public:
    CMSConversionProcessor(OFX::ImageEffect &instance) : ImageProcessor(instance)
    {
    }

    ~CMSConversionProcessor()
    {
    }

    void setValues(const Matrix3x3f &conversion_matrix,
                   OFX::Image *src)
    {
        _src = src;
        _nComponentsSrc = _src->getPixelComponentCount();
        _nComponentsDst = _dstImg->getPixelComponentCount();
        _conversion_matrix = conversion_matrix;
    }

private:
    Matrix3x3f _conversion_matrix;
    OFX::Image *_src;
    int _nComponentsSrc;
    int _nComponentsDst;

    void multiThreadProcessImages(const OfxRectI &procWindow, const OfxPointD &rs) OVERRIDE FINAL
    {
        OFX::unused(rs);

        int alphaDst = _nComponentsDst == 4;
        int alphaSrc = _nComponentsSrc == 4;

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
                Vector3f srcColor(srcPix);
                Vector3f dstColor = _conversion_matrix.vecmult(srcColor);
                *dstPix++ = dstColor[0];
                *dstPix++ = dstColor[1];
                *dstPix++ = dstColor[2];
                srcPix += 3;

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

bool CMSColorConversionPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
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
void CMSColorConversionPlugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    const double time = args.time;
    OFX::PixelComponentEnum srcComponents = _inputClip->getPixelComponents();

    assert(OFX_COMPONENTS_OK(dstComponents));

    // checkComponents(dstBitDepth, dstComponents);

    OFX::auto_ptr<OFX::Image> dst(_outputClip->fetchImage(time));
    OFX::auto_ptr<OFX::Image> src(_inputClip->fetchImage(time));
    if (!src.get())
    {
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }

    Primaries<float> srcPrimaries(
        (float)_redPrimary->getValueAtTime(time).x,
        (float)_redPrimary->getValueAtTime(time).y,
        (float)_greenPrimary->getValueAtTime(time).x,
        (float)_greenPrimary->getValueAtTime(time).y,
        (float)_bluePrimary->getValueAtTime(time).x,
        (float)_bluePrimary->getValueAtTime(time).y);
    PrimaryXY<float> sourceWhitePoint(
        (float)_sourceWhitePoint->getValueAtTime(time).x,
        (float)_sourceWhitePoint->getValueAtTime(time).y);
    PrimaryXY<float> destWhitePoint(
        (float)_destWhitePoint->getValueAtTime(time).x,
        (float)_destWhitePoint->getValueAtTime(time).y);

    int ca_method = _chromaticAdaptationMethod->getValue();
    Matrix3x3f ca_matrix;
    
    switch(ca_method){
        case 1:
        ca_matrix = cmccat2000_matrix;
        break;
        case 2:
        ca_matrix = ciecat02_matrix;
        break;
        case 0:
        default:
        ca_matrix = bradford_matrix;
    }

    Matrix3x3f conversion_matrix = compute_adapted_matrix(srcPrimaries, sourceWhitePoint, destWhitePoint, ca_matrix, _invert->getValueAtTime(time));

    if (getUseOpenCL() && srcComponents == OFX::ePixelComponentRGBA){
        clearPersistentMessage();
        cl::ImageFormat CLimf(CL_RGBA, CL_FLOAT);
        if (srcComponents == OFX::ePixelComponentRGB){
            CLimf.image_channel_order = CL_RGB;
        }
        
        OfxRectI srcBounds = src->getBounds();
        //OfxRectI dstBounds = dst->getBounds();

        cl::Event timer;
        cl::CommandQueue queue = cl::CommandQueue(getCurrentCLContext(), getCurrentCLDevice());
        cl::Kernel kernel_matrixop(getProgram("imgutils"), "matrix_xform");

        cl::Buffer matrixbuffer(getCurrentCLContext(), CL_MEM_READ_ONLY, sizeof(float) * 9);
        cl::Image2D img_in(getCurrentCLContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, CLimf, srcBounds.x2, srcBounds.y2, 0, (float*)src->getPixelData());
        cl::Image2D img_out(getCurrentCLContext(), CL_MEM_WRITE_ONLY, cl::ImageFormat(CL_RGBA, CL_FLOAT), srcBounds.x2, srcBounds.y2, 0, NULL);

        // Set CL arguments
        kernel_matrixop.setArg(0, img_in);
        kernel_matrixop.setArg(1, img_out);
        kernel_matrixop.setArg(2, matrixbuffer);

        cl::NDRange sizes(srcBounds.x2, srcBounds.y2, 1);
        int ok = queue.enqueueWriteBuffer(matrixbuffer, CL_TRUE, 0, sizeof(float) * 9, (float*)conversion_matrix.data());
        printf(">>A %i %p %i %i\n", ok, src->getPixelData(), srcBounds.x2, srcBounds.y2);
        ok = queue.enqueueNDRangeKernel(kernel_matrixop, cl::NullRange, sizes, cl::NullRange, NULL, &timer);
        printf(">> %i %p %i %i\n", ok, src->getPixelData(), srcBounds.x2, srcBounds.y2);

        // Fetch result from GPU
        cl::array<size_t, 3> origin = {(size_t)args.renderWindow.x1, (size_t)args.renderWindow.y1, 0};
        cl::array<size_t, 3> size = {(size_t)(args.renderWindow.x2 - args.renderWindow.x1), (size_t)(args.renderWindow.y2 - args.renderWindow.y1), 1};
        ok = queue.enqueueReadImage(img_out, CL_TRUE, origin, size, 0, 0, (float*)dst->getPixelData());
        printf(">> %i\n", ok);
        queue.finish();
    } else {
        if(getUseOpenCL()){
            setPersistentMessage(OFX::Message::eMessageError, "", std::string("OpenCL : Only work with RGBA images"));
        } else {
            clearPersistentMessage();
        }
        CMSConversionProcessor processor(*this);
        processor.setDstImg(dst.get());
        processor.setRenderWindow(args.renderWindow, args.renderScale);
        processor.setValues(conversion_matrix, src.get());
        processor.process();
    }
}

void CMSColorConversionPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    if (!_inputClip->isConnected())
    {
        return;
    }
    OfxRectI format;
    _inputClip->getFormat(format);
    double par = 1.;
    clipPreferences.setOutputFormat(format);
    clipPreferences.setPixelAspectRatio(*_outputClip, par);
    clipPreferences.setClipBitDepth(*_outputClip, OFX::eBitDepthFloat);
    clipPreferences.setClipComponents(*_outputClip, OFX::ePixelComponentRGBA);

    // output is continuous
    clipPreferences.setOutputHasContinuousSamples(true);
}

void CMSColorConversionPlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{
    OpenCLBase::changedParamCL(this, args, paramName);

    if (paramName == kPrimariesChoice){
        int primaries = _primariesChoice->getValue();
        OfxPointD xy;
        // Rec709
        if (primaries == 0){
            xy.x = 0.640;xy.y = 0.330;
            _redPrimary->setValue(xy);
            xy.x = 0.300;xy.y = 0.600;
            _greenPrimary->setValue(xy);
            xy.x = 0.150;xy.y = 0.060;
            _bluePrimary->setValue(xy);
        }

        // Rec2020
        if (primaries == 1){
            xy.x = 0.708;xy.y = 0.292;
            _redPrimary->setValue(xy);
            xy.x = 0.170;xy.y = 0.797;
            _greenPrimary->setValue(xy);
            xy.x = 0.131;xy.y = 0.046;
            _bluePrimary->setValue(xy);
        }

        // P3
        if (primaries == 2){
            xy.x = 0.680;xy.y = 0.320;
            _redPrimary->setValue(xy);
            xy.x = 0.262;xy.y = 0.690;
            _greenPrimary->setValue(xy);
            xy.x = 0.150;xy.y = 0.060;
            _bluePrimary->setValue(xy);
        }

        if (primaries == 3){
            xy.x = 0.684;xy.y = 0.313;
            _redPrimary->setValue(xy);
            xy.x = 0.212;xy.y = 0.722;
            _greenPrimary->setValue(xy);
            xy.x = 0.149;xy.y = 0.054;
            _bluePrimary->setValue(xy);
        }
    }

    if (paramName == kSourceWhiteChoice){
        int wb = _srcWBChoice->getValue();
        OfxPointD xy;
        if (wb == 0){
            xy.x = 0.345704;xy.y = 0.358540;
            _sourceWhitePoint->setValue(xy);
        }
        if (wb == 1){
            xy.x = 0.3127;xy.y = 0.3290;
            _sourceWhitePoint->setValue(xy);
        }
        if (wb == 2){
            xy.x = 0.303;xy.y = 0.317;
            _sourceWhitePoint->setValue(xy);
        }
    }

    if (paramName == kTargetWhiteChoice){
        int wb = _tgtWBChoice->getValue();
        OfxPointD xy;
        if (wb == 0){
            xy.x = 0.345704;xy.y = 0.358540;
            _destWhitePoint->setValue(xy);
        }
        if (wb == 1){
            xy.x = 0.3127;xy.y = 0.3290;
            _destWhitePoint->setValue(xy);
        }
    }
}

void CMSColorConversionPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
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

bool CMSColorConversionPlugin::isIdentity(const OFX::IsIdentityArguments &args,
                                          OFX::Clip *&identityClip,
                                          double & /*identityTime*/
                                          ,
                                          int & /*view*/, std::string & /*plane*/)
{
    if (0)
    {
        identityClip = _inputClip;
        return true;
    }
    return false;
}

void CMSColorConversionPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
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

    OpenCLBase::describeInContextCL(desc, context, page);

    {

        {
            OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kChromaticAdaptationMethod);
            param->setLabel("Chrmomatic adaptation method");
            param->setHint("Select prefered chromatic adaptation method");
            param->appendOption("Bradford");
            param->appendOption("CMCCAT2000");
            param->appendOption("CIECAT02");
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kPrimariesChoice);
            param->setLabel("Source primaries");
            param->setHint("Select predefined primaries");
            param->appendOption("Rec709");
            param->appendOption("Rec2020");
            param->appendOption("P3");
            param->appendOption("HP DreamColor Z27");
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::Double2DParamDescriptor *param = desc.defineDouble2DParam(kRedPrimaryParam);
            param->setLabel("Red primary");
            param->setHint("The XY coordinates of the red primary in the CIE 1931 color space");
            param->setDefault(0.64, 0.33);
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::Double2DParamDescriptor *param = desc.defineDouble2DParam(kGreenPrimaryParam);
            param->setLabel("Green primary");
            param->setHint("The XY coordinates of the green primary in the CIE 1931 color space");
            param->setDefault(0.30, 0.60);
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::Double2DParamDescriptor *param = desc.defineDouble2DParam(kBluePrimaryParam);
            param->setLabel("Blue primary");
            param->setHint("The XY coordinates of the blue primary in the CIE 1931 color space");
            param->setDefault(0.15, 0.06);
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kSourceWhiteChoice);
            param->setLabel("Source white");
            param->setHint("Select source white XY");
            param->appendOption("D50");
            param->appendOption("D65");
            param->appendOption("HP DreamColor Z27");
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::Double2DParamDescriptor *param = desc.defineDouble2DParam(kSourceWhitePointParam);
            param->setLabel("Source white primary");
            param->setHint("The XY coordinates of the source white primary in the CIE 1931 color space");
            param->setDefault(0.3127, 0.3290);
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kTargetWhiteChoice);
            param->setLabel("Destination white XY");
            param->setHint("Select white");
            param->appendOption("D50");
            param->appendOption("D65");
            if (page)
            {
                page->addChild(*param);
            }
        }
        

        {
            OFX::Double2DParamDescriptor *param = desc.defineDouble2DParam(kDestWhitePointParam);
            param->setLabel("Target white primary");
            param->setHint("The XY coordinates of the target white primary in the CIE 1931 color space");
            param->setDefault(0.345704, 0.358540);
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kInvertTransformParam);
            param->setLabel("Invert transform");
            param->setHint("If checked, the color transformation will be inverted (XYZ to RGB instead of RGB to XYZ)");
            param->setDefault(false);
            if (page)
            {
                page->addChild(*param);
            }
        }

    }
}

OFX::ImageEffect *
CMSColorConversionPluginFactory::createInstance(OfxImageEffectHandle handle,
                                                OFX::ContextEnum /*context*/)
{
    return new CMSColorConversionPlugin(handle);
}

static CMSColorConversionPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

    OFXS_NAMESPACE_ANONYMOUS_EXIT
