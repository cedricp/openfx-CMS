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

#include "../RawLib/mlv_video.h"

#define kPluginName "CMSMLVReader"
#define kPluginGrouping "MagicLantern"
#define kPluginDescription                                                                                                                                                                                                                                             \
    "Magic lantern MLV reader plugin"

#define kPluginIdentifier "net.sf.openfx.CMSMLVReader"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 0
#define kSupportsMultiResolution 0
#define kSupportsRenderScale 0
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false
#define kRenderThreadSafety eRenderFullySafe

#define kMLVfileParamter "MLVFilename"


OFXS_NAMESPACE_ANONYMOUS_ENTER

#ifdef OFX_EXTENSIONS_NATRON
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#else
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#endif

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class CMSMLVReaderPlugin: public GeneratorPlugin
{
public:
    /** @brief ctor */
    CMSMLVReaderPlugin(OfxImageEffectHandle handle)
        : GeneratorPlugin(handle, true, false, true, false, false)
    {
        _mlv_video = nullptr;
        _mlvfilename_param = fetchStringParam(kMLVfileParamter);
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;
    virtual void changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) OVERRIDE FINAL;
    bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;

private:
    Mlv_video* _mlv_video;
    OFX::StringParam* _mlvfilename_param;
};

mDeclarePluginFactory(CMSMLVReaderPluginFactory, { OFX::ofxsThreadSuiteCheck(); }, {});

OFXS_NAMESPACE_ANONYMOUS_EXIT