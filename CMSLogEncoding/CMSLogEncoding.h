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

#define kPluginName "CMSLogEncodingOFX"
#define kPluginGrouping "CMSPlugin"
#define kPluginDescription                                                                                                                                                                                                                                             \
    "Log2 allocation utility."

#define kPluginIdentifier "net.sf.openfx.CMSLogEncoding"
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

OFXS_NAMESPACE_ANONYMOUS_ENTER

#ifdef OFX_EXTENSIONS_NATRON
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentAlpha || (c) == OFX::ePixelComponentXY || (c) == OFX::ePixelComponentRGB || (c) == OFX::ePixelComponentRGBA)
#else
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentAlpha || (c) == OFX::ePixelComponentRGB || (c) == OFX::ePixelComponentRGBA)
#endif

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class CMSLogEncodingPlugin: public GeneratorPlugin
{
public:
    /** @brief ctor */
    CMSLogEncodingPlugin(OfxImageEffectHandle handle)
        : GeneratorPlugin(handle, true, kSupportsByte, kSupportsUShort, kSupportsHalf, kSupportsFloat)
    {
        _inputClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        _outputClip = fetchClip(kOfxImageEffectOutputClipName);
        _isAntiLog = fetchBooleanParam("antiLog");
        _logminmax = fetchDouble2DParam("log2 min max");
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
    OFX::Clip* _inputClip;
    OFX::Clip* _outputClip;
    OFX::BooleanParam *_isAntiLog;
    OFX::Double2DParam *_logminmax;
};

mDeclarePluginFactory(CMSLogEncodingPluginFactory, { OFX::ofxsThreadSuiteCheck(); }, {});

OFXS_NAMESPACE_ANONYMOUS_EXIT