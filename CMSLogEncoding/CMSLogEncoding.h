#pragma once

#include "ofxsProcessing.H"
#include "ofxsMacros.h"
#include "ofxsImageEffect.h"
#include "ofxsLut.h"
#include "ofxsCoords.h"
#ifdef OFX_EXTENSIONS_NATRON
#include "ofxNatron.h"
#endif
#include "ofxsThreadSuite.h"

#define kPluginName "CMSLogEncodingOFX"
#define kPluginGrouping "CMSPlugins"
#define kPluginDescription "Log2 allocation utility."

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

#define kParamAntilog "antiLog"
#define kParamMinMax "log2minmax"

OFXS_NAMESPACE_ANONYMOUS_ENTER

#ifdef OFX_EXTENSIONS_NATRON
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB || (c) == OFX::ePixelComponentRGBA)
#else
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB || (c) == OFX::ePixelComponentRGBA)
#endif

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class CMSLogEncodingPlugin: public OFX::ImageEffect
{
public:
    /** @brief ctor */
    CMSLogEncodingPlugin(OfxImageEffectHandle handle) : OFX::ImageEffect(handle)
    {
        _inputClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        _outputClip = fetchClip(kOfxImageEffectOutputClipName);
        _isAntiLog = fetchBooleanParam(kParamAntilog);
        _logminmax = fetchDouble2DParam(kParamMinMax);
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;
    bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
    void changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) OVERRIDE FINAL;
    bool isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip * &identityClip, double &identityTime, int&view, std::string& plane) OVERRIDE FINAL;

private:
    int _lutSize;
    OFX::Clip* _inputClip;
    OFX::Clip* _outputClip;
    OFX::StringParam* _outputLutFile;
    OFX::BooleanParam *_isAntiLog;
    OFX::Double2DParam *_logminmax;
};

class CMSLogEncodingPluginFactory : public OFX::PluginFactoryHelper<CMSLogEncodingPluginFactory>
{
public:
    CMSLogEncodingPluginFactory(const std::string& id, unsigned int verMaj, unsigned int verMin):OFX::PluginFactoryHelper<CMSLogEncodingPluginFactory>(id, verMaj, verMin)
    {}
    virtual void load()
    { OFX::ofxsThreadSuiteCheck(); }
    virtual void unload() {}
    virtual void describe(OFX::ImageEffectDescriptor &desc);
    virtual void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context); virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context); 
};

OFXS_NAMESPACE_ANONYMOUS_EXIT