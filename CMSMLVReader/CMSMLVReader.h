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

#include <mlv_video.h>
#include <dng_convert.h>

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
#define kRenderThreadSafety eRenderInstanceSafe

#define kMLVfileParamter "MLVFilename"
#define kColorSpaceFormat "ColorSpaceFormat"
#define kColorTemperature "ColorTemperature"
#define kCameraWhiteBalance "CameraWhiteBalance"

OFXS_NAMESPACE_ANONYMOUS_ENTER

#ifdef OFX_EXTENSIONS_NATRON
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#else
#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)
#endif

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class CMSMLVReaderPlugin: public OFX::ImageEffect
{
public:
    /** @brief ctor */
    CMSMLVReaderPlugin(OfxImageEffectHandle handle) : OFX::ImageEffect(handle)
    {
        _mlv_video = nullptr;
        _mlvfilename_param = fetchStringParam(kMLVfileParamter);
        _outputClip = fetchClip(kOfxImageEffectOutputClipName);
        _colorSpaceFormat = fetchChoiceParam(kColorSpaceFormat);
        _colorTemperature = fetchIntParam(kColorTemperature);
        _cameraWhiteBalance = fetchBooleanParam(kCameraWhiteBalance);
        if (_mlvfilename_param->getValue().empty() == false) {
            setMlvFile(_mlvfilename_param->getValue());
        }
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;
    virtual void changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) OVERRIDE FINAL;
    virtual bool getTimeDomain(OfxRangeD& range) OVERRIDE FINAL;
    bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
    void setMlvFile(std::string file);
    virtual bool isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& identityClip, double& identityTime, int& view, std::string& plane) OVERRIDE;
    virtual bool isVideoStream(const std::string& filename){return true;};

private:
    OFX::Clip* _outputClip;
    Mlv_video* _mlv_video;
    Mlv_video::RawInfo _rawInfo;
    std::string _mlvfilename;
    OFX::StringParam* _mlvfilename_param;
    OFX::ChoiceParam* _colorSpaceFormat;
    OFX::IntParam* _colorTemperature;
    OFX::BooleanParam* _cameraWhiteBalance;
};

mDeclarePluginFactory(CMSMLVReaderPluginFactory, { OFX::ofxsThreadSuiteCheck(); }, {});

OFXS_NAMESPACE_ANONYMOUS_EXIT