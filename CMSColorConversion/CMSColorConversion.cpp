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

static std::string string_format(const std::string fmt_str, ...) {
    int final_n, n = ((int)fmt_str.size()) * 2; /* Reserve two times as much as the length of the fmt_str */
    std::unique_ptr<char[]> formatted;
    va_list ap;
    while(1) {
        formatted.reset(new char[n]); /* Wrap the plain char array into the unique_ptr */
        strcpy(&formatted[0], fmt_str.c_str());
        va_start(ap, fmt_str);
        final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
        va_end(ap);
        if (final_n < 0 || final_n >= n)
            n += abs(final_n - n + 1);
        else
            break;
    }
    return std::string(formatted.get());
}

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
                   OFX::Image *src, int trc, bool inverse)
    {
        _src = src;
        _nComponentsSrc = _src->getPixelComponentCount();
        _nComponentsDst = _dstImg->getPixelComponentCount();
        _conversion_matrix = conversion_matrix;
        _inverse = inverse;
        _toneResponseCurve = trc;
    }

private:
    Matrix3x3f _conversion_matrix;
    OFX::Image *_src;
    int _nComponentsSrc;
    int _nComponentsDst;
    bool _inverse;
    int _toneResponseCurve;

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
            if (!dstPix)
            {
                continue;
            }
            
            for (int x = procWindow.x1; x < procWindow.x2; ++x)
            {
                const float *srcPix = (float *)_src->getPixelAddress(x, y);
                Vector3f srcColor(srcPix);
                if (!_inverse){
                    switch(_toneResponseCurve)
                    {
                        case 0: // Linear
                            break;
                        case 1: // Gamma 2.2
                            srcColor = inverse_gamma_correct(srcColor, 2.2);
                            break;
                        case 2: // Gamma 2.4
                            srcColor = inverse_gamma_correct(srcColor, 2.4);
                            break;
                        case 3: // Gamma 2.6
                            srcColor = inverse_gamma_correct(srcColor, 2.6);
                            break;
                        case 4: // sRGB
                            srcColor = srgb_to_linear(srcColor);
                            break;
                    }
                }

                if (!srcPix) continue;
                Vector3f dstColor = _conversion_matrix * srcColor;
                
                if (_inverse){
                    switch(_toneResponseCurve)
                    {
                        case 0: // Linear
                            break;
                        case 1: // Gamma 2.2
                            dstColor = gamma_correct(dstColor, 2.2);
                        break;
                        case 2: // Gamma 2.4
                            dstColor = gamma_correct(dstColor, 2.4);
                        break;
                        case 3: // Gamma 2.6
                            dstColor = gamma_correct(dstColor, 2.6);
                            break;
                        case 4: // sRGB
                            dstColor = linear_to_srgb(dstColor);
                            break;
                    }
                }
                
                *dstPix++ = dstColor[0];
                *dstPix++ = dstColor[1];
                *dstPix++ = dstColor[2];

                if (alphaDst && alphaSrc)
                {
                    *dstPix++ = *srcPix++;
                }
                else if (alphaDst)
                {
                    *dstPix++ = 1.0;
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

    RGBWPrimaries<float> srcPrimaries(
        (float)_redPrimary->getValueAtTime(time).x,
        (float)_redPrimary->getValueAtTime(time).y,
        (float)_greenPrimary->getValueAtTime(time).x,
        (float)_greenPrimary->getValueAtTime(time).y,
        (float)_bluePrimary->getValueAtTime(time).x,
        (float)_bluePrimary->getValueAtTime(time).y,
        (float)_sourceWhitePoint->getValueAtTime(time).x,
        (float)_sourceWhitePoint->getValueAtTime(time).y);

    PrimariesXY<float> destWhitePoint(
        (float)_destWhitePoint->getValueAtTime(time).x,
        (float)_destWhitePoint->getValueAtTime(time).y);

    int ca_method = _chromaticAdaptationMethod->getValue();
    Matrix3x3f ca_matrix;
    
    switch(ca_method){
        case 1:
        ca_matrix = cmccat2000_matrix<float>;
        break;
        case 2:
        ca_matrix = ciecat02_matrix<float>;
        break;
        case 0:
        default:
        ca_matrix = bradford_matrix<float>;
    }

    bool adaptation_only = _primariesChoice->getValue() == 10;
    bool invert = _invert->getValueAtTime(time);
    int trc = _toneResponseCurve->getValue();
    Matrix3x3f conversion_matrix;

    if (!adaptation_only)
    {
       conversion_matrix = srcPrimaries.compute_adapted_rgb2xyz_matrix(invert, destWhitePoint, ca_matrix);
    } else
    {
        trc = 0;
        conversion_matrix = srcPrimaries.compute_chromatic_adaptation_matrix(destWhitePoint, ca_matrix);
        if (invert) conversion_matrix.invert_in_place();
    }

    conversion_matrix.print("Conversion matrix");

    if (getUseOpenCL() && srcComponents == OFX::ePixelComponentRGBA)
    {
        clearPersistentMessage();
        
        OfxRectI srcBounds = src->getBounds();
        OfxRectI dstBounds = dst->getBounds();

        int srcWidth = srcBounds.x2 - srcBounds.x1;
        int srcHeight = srcBounds.y2 - srcBounds.y1;
        int dstWidth = dstBounds.x2 - dstBounds.x1;
        int dstHeight = dstBounds.y2 - dstBounds.y1;

        cl::Event timer;
        cl::CommandQueue queue = cl::CommandQueue(getCurrentCLContext(), getCurrentCLDevice());
        cl::Kernel kernel_matrixop(getProgram("imgutils"), "matrix_xform");

        cl::Buffer matrixbuffer(getCurrentCLContext(), CL_MEM_READ_ONLY, sizeof(float) * 9);
        cl::Image2D img_in(getCurrentCLContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_RGBA, CL_FLOAT), srcWidth, srcHeight, 0, (float*)src->getPixelData());
        cl::Image2D img_out(getCurrentCLContext(), CL_MEM_WRITE_ONLY, cl::ImageFormat(CL_RGBA, CL_FLOAT), dstWidth, dstHeight, 0, NULL);

        // Set CL arguments
        kernel_matrixop.setArg(0, img_in);
        kernel_matrixop.setArg(1, img_out);
        kernel_matrixop.setArg(2, matrixbuffer);
        kernel_matrixop.setArg(3, (int)srcBounds.x1);
        kernel_matrixop.setArg(4, (int)srcBounds.y1);
        kernel_matrixop.setArg(5, (int)srcWidth);
        kernel_matrixop.setArg(6, (int)srcHeight);
        kernel_matrixop.setArg(7, (int)trc);
        kernel_matrixop.setArg(8, (int)invert);

        cl::NDRange sizes(srcBounds.x2, srcBounds.y2, 1);
        int errcode = queue.enqueueWriteBuffer(matrixbuffer, CL_TRUE, 0, sizeof(float) * 9, (float*)conversion_matrix.data());
        if (errcode != CL_SUCCESS)
        {
            setPersistentMessage(OFX::Message::eMessageError, "", string_format("OpenCL : Enqueue buffer write failed with error code %i", errcode));
            return;
        }
        errcode = queue.enqueueNDRangeKernel(kernel_matrixop, cl::NullRange, sizes, cl::NullRange, NULL, &timer);
        if (errcode != CL_SUCCESS)
        {
            setPersistentMessage(OFX::Message::eMessageError, "", string_format("OpenCL : enqueueNDRangeKernel failed with error code %i", errcode));
            return;
        }

        // Fetch result from GPU
        cl::array<size_t, 3> origin = {(size_t)args.renderWindow.x1, (size_t)args.renderWindow.y1, 0};
        cl::array<size_t, 3> size = {(size_t)(args.renderWindow.x2 - args.renderWindow.x1), (size_t)(args.renderWindow.y2 - args.renderWindow.y1), 1};
        errcode = queue.enqueueReadImage(img_out, CL_TRUE, origin, size, 0, 0, (float*)dst->getPixelData());
        if (errcode != CL_SUCCESS)
        {
            setPersistentMessage(OFX::Message::eMessageError, "", string_format("OpenCL : enqueueReadImage failed with error code %i", errcode));
        }
        queue.finish();
    }
    else 
    {
        if(getUseOpenCL()){
            setPersistentMessage(OFX::Message::eMessageWarning, "", std::string("OpenCL : Only work with RGBA images, continuing with CPU processing"));
        } else {
            clearPersistentMessage();
        }
        CMSConversionProcessor processor(*this);
        processor.setDstImg(dst.get());
        processor.setRenderWindow(args.renderWindow, args.renderScale);
        processor.setValues(conversion_matrix, src.get(), trc, invert);
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
        _toneResponseCurve->setEnabled(true);
        int primaries = _primariesChoice->getValue();
        OfxPointD xy;
        _redPrimary->setEnabled(false);
        _greenPrimary->setEnabled(false);
        _bluePrimary->setEnabled(false);
        // Rec709
        if (primaries == 0){
            RGBWPrimaries rec709d65 = rec709d65_primaries<double>;
            _redPrimary->setValue(rec709d65.red_primaries().x(), rec709d65_primaries<float>.red_primaries().y());
            _greenPrimary->setValue(rec709d65.green_primaries().x(), rec709d65.green_primaries().y());
            _bluePrimary->setValue(rec709d65.blue_primaries().x(), rec709d65.blue_primaries().y());
        }

        // Rec2020
        if (primaries == 1){
            RGBWPrimaries rec2020d65 = rec2020d65_primaries<double>;
            _redPrimary->setValue(rec2020d65.red_primaries().x(), rec2020d65.red_primaries().y());
            _greenPrimary->setValue(rec2020d65.green_primaries().x(), rec2020d65.green_primaries().y());
            _bluePrimary->setValue(rec2020d65.blue_primaries().x(), rec2020d65.blue_primaries().y());
        }

        // P3
        if (primaries == 2){
            RGBWPrimaries p3d65 = p3_display_primaries<double>;
            _redPrimary->setValue(p3d65.red_primaries().x(), p3d65.red_primaries().y());
            _greenPrimary->setValue(p3d65.green_primaries().x(), p3d65.green_primaries().y());
            _bluePrimary->setValue(p3d65.blue_primaries().x(), p3d65.blue_primaries().y());
        }

        // Pal/Secam
        if (primaries == 3){
            RGBWPrimaries bt601 = bt601_primaries<double>;
            _redPrimary->setValue(bt601.red_primaries().x(), bt601.red_primaries().y());
            _greenPrimary->setValue(bt601.green_primaries().x(), bt601.green_primaries().y());
            _bluePrimary->setValue(bt601.blue_primaries().x(), bt601.blue_primaries().y());
        }

        // Wide Gamut RGB
        if (primaries == 4){
            RGBWPrimaries widegamut = wide_gamut_rgb_primaries<double>;
            _redPrimary->setValue(widegamut.red_primaries().x(), widegamut.red_primaries().y());
            _greenPrimary->setValue(widegamut.green_primaries().x(), widegamut.green_primaries().y());
            _bluePrimary->setValue(widegamut.blue_primaries().x(), widegamut.blue_primaries().y());
        }

        // Adobe RGB
        if (primaries == 5){
            RGBWPrimaries adobe_rgb = adobe_rgb_primaries<double>;
            _redPrimary->setValue(adobe_rgb.red_primaries().x(), adobe_rgb.red_primaries().y());
            _greenPrimary->setValue(adobe_rgb.green_primaries().x(), adobe_rgb.green_primaries().y());
            _bluePrimary->setValue(adobe_rgb.blue_primaries().x(), adobe_rgb.blue_primaries().y());
        }

        // ACES AP0
        if (primaries == 6){
            RGBWPrimaries ap0_dreamcolor = ap0_primaries<double>;
            _redPrimary->setValue(ap0_dreamcolor.red_primaries().x(), ap0_dreamcolor.red_primaries().y());
            _greenPrimary->setValue(ap0_dreamcolor.green_primaries().x(), ap0_dreamcolor.green_primaries().y());
            _bluePrimary->setValue(ap0_dreamcolor.blue_primaries().x(), ap0_dreamcolor.blue_primaries().y());
        }

        // ACES AP1
        if (primaries == 7){
            RGBWPrimaries ap1_dreamcolor = ap1_primaries<double>;
            _redPrimary->setValue(ap1_dreamcolor.red_primaries().x(), ap1_dreamcolor.red_primaries().y());
            _greenPrimary->setValue(ap1_dreamcolor.green_primaries().x(), ap1_dreamcolor.green_primaries().y());
            _bluePrimary->setValue(ap1_dreamcolor.blue_primaries().x(), ap1_dreamcolor.blue_primaries().y());
        }

        // HP Z27 DreamColor
        if (primaries == 8){
            RGBWPrimaries hp_dreamcolor = hp_dreamcolor_g1_rgb_primaries<double>;
            _redPrimary->setValue(hp_dreamcolor.red_primaries().x(), hp_dreamcolor.red_primaries().y());
            _greenPrimary->setValue(hp_dreamcolor.green_primaries().x(), hp_dreamcolor.green_primaries().y());
            _bluePrimary->setValue(hp_dreamcolor.blue_primaries().x(), hp_dreamcolor.blue_primaries().y());
        }

        
        // HP Z27 DreamColor
        if (primaries == 9){
            RGBWPrimaries hp_dreamcolor = hp_dreamcolor_g2_rgb_primaries<double>;
            _redPrimary->setValue(hp_dreamcolor.red_primaries().x(), hp_dreamcolor.red_primaries().y());
            _greenPrimary->setValue(hp_dreamcolor.green_primaries().x(), hp_dreamcolor.green_primaries().y());
            _bluePrimary->setValue(hp_dreamcolor.blue_primaries().x(), hp_dreamcolor.blue_primaries().y());
        }

        // Chromatic Adaptation
        if (primaries == 10){
            _redPrimary->setValue(0,0);
            _greenPrimary->setValue(0,0);
            _bluePrimary->setValue(0,0);
            _toneResponseCurve->setValue(0);
            _toneResponseCurve->setEnabled(false);
        }

        if (primaries == 11){
            // Custom
            _redPrimary->setEnabled(true);
            _greenPrimary->setEnabled(true);
            _bluePrimary->setEnabled(true);
        }
    }

    if (paramName == kSourceWhiteChoice){
        int wb = _srcWBChoice->getValue();
        _sourceWhitePoint->setEnabled(false);
        OfxPointD xy;
        if (wb == 0){
            // D50
            _sourceWhitePoint->setValue(WP_D50<double>.x(), WP_D50<double>.y());
        }
        if (wb == 1){
            // D65
            _sourceWhitePoint->setValue(WP_D65<double>.x(), WP_D65<double>.y());
        }
        if (wb == 2){
            // DCI P3
            _sourceWhitePoint->setValue(WP_P3_DCI<double>.x(), WP_P3_DCI<double>.y());
        }
        if (wb == 3){
            // DreamColor Z27
            _sourceWhitePoint->setValue(WP_ACES<double>.x(), WP_ACES<double>.y());
        }
        if (wb == 4){
            // DreamColor Z27
            _sourceWhitePoint->setValue(WP_DISPLAY_G1_DREAMCOLOR<double>.x(), WP_DISPLAY_G1_DREAMCOLOR<double>.y());
        }
        if (wb == 5){
            // DreamColor Z27
            _sourceWhitePoint->setValue(WP_DISPLAY_G2_DREAMCOLOR<double>.x(), WP_DISPLAY_G2_DREAMCOLOR<double>.y());
        }
        if (wb == 6){
            // Custom
            _sourceWhitePoint->setEnabled(true);
        }
    }

    if (paramName == kTargetWhiteChoice){
        int wb = _tgtWBChoice->getValue();
        _destWhitePoint->setEnabled(false);
        OfxPointD xy;
        if (wb == 0){
            // D50
            _destWhitePoint->setValue(WP_D50<double>.x(), WP_D50<double>.y());
        }
        if (wb == 1){
            // D65
            _destWhitePoint->setValue(WP_D65<double>.x(), WP_D65<double>.y());
        }
        if (wb == 2){
            // DCI P3
            _destWhitePoint->setValue(WP_P3_DCI<double>.x(), WP_P3_DCI<double>.y());
        }
        if (wb == 3){
            // DreamColor Z27
            _destWhitePoint->setValue(WP_ACES<double>.x(), WP_ACES<double>.y());
        }
        if (wb == 4){
            // DreamColor Z27
            _destWhitePoint->setValue(WP_DISPLAY_G1_DREAMCOLOR<double>.x(), WP_DISPLAY_G1_DREAMCOLOR<double>.y());
        }
        if (wb == 5){
            // DreamColor Z27
            _destWhitePoint->setValue(WP_DISPLAY_G2_DREAMCOLOR<double>.x(), WP_DISPLAY_G2_DREAMCOLOR<double>.y());
        }
        if (wb == 6){
            // Custom
            _destWhitePoint->setEnabled(true);
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

bool CMSColorConversionPlugin::isIdentity(const OFX::IsIdentityArguments &/*args*/,
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

void setWPParam(OFX::ChoiceParamDescriptor* param)
{
    param->appendOption("D50");
    param->appendOption("D65");
    param->appendOption("DCI-P3 (~6300K)");
    param->appendOption("D60 (ACES)");
    param->appendOption("HP DreamColor Z27G1");
    param->appendOption("HP DreamColor Z27G2");
    param->appendOption("Custom");
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
            OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kToneResponseCurve);
            param->setLabel("Tone response curve");
            param->setHint("Select tone response curve");
            param->appendOption("Linear");
            param->appendOption("Gamma 2.2");
            param->appendOption("Gamma 2.4");
            param->appendOption("Gamma 2.6");
            param->appendOption("sRGB");
            if (page)
            {
                page->addChild(*param);
            }
        }

        {
            OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kPrimariesChoice);
            param->setLabel("Source primaries");
            param->setHint("Select predefined RGB primaries");
            param->appendOption("Rec709");
            param->appendOption("Rec2020");
            param->appendOption("P3");
            param->appendOption("BT601 (Pal/Secam)");
            param->appendOption("Wide gamut");
            param->appendOption("Adobe RGB");
            param->appendOption("ACES AP0");
            param->appendOption("ACES AP1");
            param->appendOption("HP DreamColor Z27G1");
            param->appendOption("HP DreamColor Z27G2");
            param->appendOption("XYZ (Chormatic adaptation only)");
            param->appendOption("Custom");
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
            setWPParam(param);
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
            setWPParam(param);
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
