#pragma once

#include "ofxsProcessing.H"
#include "ofxsMacros.h"
#include "ofxsGenerator.h"
#include "ofxsLut.h"
#include "ofxsCoords.h"
#ifdef OFX_EXTENSIONS_NATRON
#include "ofxNatron.h"
#endif
#include "ofxsThreadSuite.h"

#define kPluginName "CMSPatternOFX"
#define kPluginGrouping "CMSPlugin"
#define kPluginDescription                                                                                                                                                                                                                                             \
    "Generate an image for 3D LUT creation"

#define kPluginIdentifier "net.sf.openfx.CMSPattern"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsByte true
#define kSupportsUShort true
#define kSupportsHalf false
#define kSupportsFloat true

#define kSupportsTiles 1
#define kSupportsMultiResolution 0
#define kSupportsRenderScale 0
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false
#define kRenderThreadSafety eRenderFullySafe

#define kParamLUTSize "LUTSize"
#define kParamBarIntensityLabel "LUT Size"
#define kParamBarIntensityHint "CMS Pattern LUT size."
#define kParamBarIntensityDefault 16

OFXS_NAMESPACE_ANONYMOUS_ENTER

#ifdef OFX_EXTENSIONS_NATRON
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#else
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#endif

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class CMSPatternPlugin: public GeneratorPlugin
{
public:
    /** @brief ctor */
    CMSPatternPlugin(OfxImageEffectHandle handle)
        : GeneratorPlugin(handle, true, kSupportsByte, kSupportsUShort, kSupportsHalf, kSupportsFloat)
    {
        _lutSize = fetchIntParam(kParamLUTSize);
        _antiLogScale = fetchBooleanParam("log2 encode");
        _logminmax = fetchDouble2DParam("log2 min max");
        assert(_lutSize);
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;
    bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
    OfxPointI getCMSResolution();

private:
    OFX::IntParam *_lutSize;
    OFX::BooleanParam *_antiLogScale;
    OFX::Double2DParam *_logminmax;
};

mDeclarePluginFactory(CMSPatternPluginFactory, { OFX::ofxsThreadSuiteCheck(); }, {});

OFXS_NAMESPACE_ANONYMOUS_EXIT