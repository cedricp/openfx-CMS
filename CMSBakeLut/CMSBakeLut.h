#pragma once

#include "ofxsProcessing.H"
#include "ofxsMacros.h"
#include "ofxsImageEffect.h"
#include "ofxsCoords.h"
#ifdef OFX_EXTENSIONS_NATRON
#include "ofxNatron.h"
#endif
#include "ofxsThreadSuite.h"

#define kPluginName "CMSBakeLutOFX"
#define kPluginGrouping "CMSPlugins"
#define kPluginDescription "Generate a 3D LUT from a CMS pattern."

#define kPluginIdentifier "net.sf.openfx.CMSBakeLut"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 0
#define kSupportsMultiResolution 0
#define kSupportsRenderScale 0
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false

#define kParamEnableShaperLut "enable_shaper_lut"
#define kParamLogMinmax "log2 min max"
#define kParamShaperSize "lut1dsize"

OFXS_NAMESPACE_ANONYMOUS_ENTER

#ifdef OFX_EXTENSIONS_NATRON
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#else
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#endif

struct Color
{
    float r, g, b;
};

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class CMSBakeLutPlugin: public OFX::ImageEffect
{
public:
    /** @brief ctor */
    CMSBakeLutPlugin(OfxImageEffectHandle handle) : OFX::ImageEffect(handle)
    {
        _inputClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);

        _outputLutFile = fetchStringParam(kOfxImageEffectFileParamName);
        _logScale = fetchBooleanParam(kParamEnableShaperLut);
        _logminmax = fetchDouble2DParam(kParamLogMinmax);
        _lut1dsize = fetchChoiceParam(kParamShaperSize);
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;
    bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
    void changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) OVERRIDE FINAL;

private:
    int _lutSize;
    OFX::StringParam* _outputLutFile;
    std::vector<Color> _lut;
    OFX::BooleanParam *_logScale;
    OFX::Double2DParam *_logminmax;
    OFX::ChoiceParam *_lut1dsize;
    OFX::Clip * _dstClip;
    OFX::Clip* _inputClip;
};

mDeclarePluginFactory(CMSBakeLutPluginFactory, { OFX::ofxsThreadSuiteCheck(); }, {});

OFXS_NAMESPACE_ANONYMOUS_EXIT