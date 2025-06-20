#pragma once

#include "ofxsProcessing.H"
#include "OpenCLBase.h"
#ifdef OFX_EXTENSIONS_NATRON
#include "ofxNatron.h"
#endif
#include "ofxsThreadSuite.h"

#define kPluginName "CMSColorConversionOFX"
#define kPluginGrouping "CMSPlugins"
#define kPluginDescription "A color transformation plugin that creates RGB to XYZ conversion from (xy) color primaries and (xy) white point."

#define kPluginIdentifier "net.sf.openfx.CMSColorConversionOFX"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsByte false
#define kSupportsUShort false
#define kSupportsHalf false
#define kSupportsFloat true

// More work is needed to support tiles
#define kSupportsTiles 0
#define kSupportsMultiResolution 0
#define kSupportsRenderScale 0
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false
#define kRenderThreadSafety eRenderFullySafe

#define kRedPrimaryParam "redPrimary"
#define kGreenPrimaryParam "greenPrimary"
#define kBluePrimaryParam "bluePrimary"
#define kSourceWhitePointParam "sourceWhitePoint"
#define kDestWhitePointParam "destWhitePoint"
#define kPrimariesChoice "primaries"
#define kSourceWhiteChoice "srcWhite"
#define kTargetWhiteChoice "tgtWhite"
#define kInvertTransformParam "invert"
#define kChromaticAdaptationMethod "chomaticAdaptation"

OFXS_NAMESPACE_ANONYMOUS_ENTER

#ifdef OFX_EXTENSIONS_NATRON
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB || (c) == OFX::ePixelComponentRGBA)
#else
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB || (c) == OFX::ePixelComponentRGBA)
#endif

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class CMSColorConversionPlugin : public OpenCLBase
{
public:
    /** @brief ctor */
    CMSColorConversionPlugin(OfxImageEffectHandle handle) : OpenCLBase(handle)
    {
        _inputClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        _outputClip = fetchClip(kOfxImageEffectOutputClipName);
        _redPrimary = fetchDouble2DParam(kRedPrimaryParam);
        _greenPrimary = fetchDouble2DParam(kGreenPrimaryParam);
        _bluePrimary = fetchDouble2DParam(kBluePrimaryParam);
        _sourceWhitePoint = fetchDouble2DParam(kSourceWhitePointParam);
        _destWhitePoint = fetchDouble2DParam(kDestWhitePointParam);
        _invert = fetchBooleanParam(kInvertTransformParam);
        _primariesChoice = fetchChoiceParam(kPrimariesChoice);
        _srcWBChoice = fetchChoiceParam(kSourceWhiteChoice);
        _tgtWBChoice = fetchChoiceParam(kTargetWhiteChoice);
        _chromaticAdaptationMethod = fetchChoiceParam(kChromaticAdaptationMethod);
        std::string debayer_program = getPluginFilePath() + "/Contents/Resources/Shaders/imgutils.cl";
        addProgram(debayer_program, "imgutils");

        _greenPrimary->setEnabled(false);
        _bluePrimary->setEnabled(false);
        _redPrimary->setEnabled(false);
        _sourceWhitePoint->setEnabled(false);
        _destWhitePoint->setEnabled(false);
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;
    bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
    void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;
    bool isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip *&identityClip, double &identityTime, int &view, std::string &plane) OVERRIDE FINAL;

private:
    int _lutSize;
    OFX::Clip *_inputClip;
    OFX::Clip *_outputClip;
    OFX::StringParam *_outputLutFile;
    OFX::Double2DParam *_redPrimary;
    OFX::Double2DParam *_greenPrimary;
    OFX::Double2DParam *_bluePrimary;
    OFX::Double2DParam *_sourceWhitePoint;
    OFX::Double2DParam *_destWhitePoint;
    OFX::BooleanParam *_invert;
    OFX::ChoiceParam *_primariesChoice;
    OFX::ChoiceParam *_srcWBChoice;
    OFX::ChoiceParam *_tgtWBChoice;
    OFX::ChoiceParam *_chromaticAdaptationMethod;
    OFX::BooleanParam *_chromaticAdaptationOnly;
};

class CMSColorConversionPluginFactory : public OFX::PluginFactoryHelper<CMSColorConversionPluginFactory>
{
public:
    CMSColorConversionPluginFactory(const std::string &id, unsigned int verMaj, unsigned int verMin) : OFX::PluginFactoryHelper<CMSColorConversionPluginFactory>(id, verMaj, verMin)
    {
    }
    virtual void load()
    {
        OFX::ofxsThreadSuiteCheck();
    }
    virtual void unload() {}
    virtual void describe(OFX::ImageEffectDescriptor &desc);
    virtual void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context);
    virtual OFX::ImageEffect *createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context);
};

OFXS_NAMESPACE_ANONYMOUS_EXIT