#pragma once

#include "ofxsProcessing.H"
#include "ofxsMacros.h"
#include "ofxsImageEffect.h"
#include "ofxsCoords.h"
#ifdef OFX_EXTENSIONS_NATRON
#include "ofxNatron.h"
#endif
#include "ofxsThreadSuite.h"

#define kPluginName "CMSPatternOFX"
#define kPluginGrouping "CMSPlugins"
#define kPluginDescription "Generate an image for 3D LUT creation"

#define kPluginIdentifier "net.sf.openfx.CMSPattern"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 1
#define kSupportsMultiResolution 0
#define kSupportsRenderScale 0
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false
#define kRenderThreadSafety eRenderFullySafe

#define kParamLUTSizeName "LUTSize"
#define kParamLutSizeLabel "LUT Size"
#define kParamLutSizeHint "CMS Pattern LUT size."
#define kParamLUTSize 16

#define kParamLog2EncodeEnable "enableLog2Encode"
#define kParamLog2MinMax "log2MinMax"

OFXS_NAMESPACE_ANONYMOUS_ENTER

#ifdef OFX_EXTENSIONS_NATRON
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#else
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#endif

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class CMSPatternPlugin: public OFX::ImageEffect
{
public:
    /** @brief ctor */
    CMSPatternPlugin(OfxImageEffectHandle handle)
        : OFX::ImageEffect(handle)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        _lutSize = fetchIntParam(kParamLUTSizeName);
    }

    virtual ~CMSPatternPlugin(){ }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;
    virtual void changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) OVERRIDE FINAL;
    bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
    virtual bool isVideoStream(const std::string& filename){return false;};
    virtual bool getTimeDomain(OfxRangeD& range) OVERRIDE FINAL;
    virtual bool isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& identityClip, double& identityTime, int& view, std::string& plane) OVERRIDE;
    
    OfxPointI getCMSResolution();
private:
    OFX::IntParam *_lutSize;
    OFX::Clip* _dstClip;
};


void loadPlugin();
class CMSPatternPluginFactory : public OFX::PluginFactoryHelper<CMSPatternPluginFactory> { 
    public:
    CMSPatternPluginFactory(const std::string& id, unsigned int verMaj, unsigned int verMin)  :OFX::PluginFactoryHelper<CMSPatternPluginFactory>(id, verMaj, verMin)
    {}
        virtual void load() { loadPlugin(); }
        virtual void unload() {} ;
        virtual void describe(OFX::ImageEffectDescriptor &desc);
        virtual void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context);
        virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context);
};

OFXS_NAMESPACE_ANONYMOUS_EXIT