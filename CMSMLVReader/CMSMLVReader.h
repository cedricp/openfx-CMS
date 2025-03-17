#pragma once

#include <cstdlib>
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
#define kRenderThreadSafety eRenderFullySafe

#define kMLVfileParamter "MLVFilename"
#define kColorSpaceFormat "ColorSpaceFormat"
#define kDebayerType "DebayerType"
#define kColorTemperature "ColorTemperature"
#define kCameraWhiteBalance "CameraWhiteBalance"
#define kHighlightMode "HighlightMode"
#define kTimeRange "Timerange"

OFXS_NAMESPACE_ANONYMOUS_ENTER

#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)

extern "C"
{
    extern char FOCUSPIXELMAPFILE[256];
}

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
        _debayerType = fetchChoiceParam(kDebayerType);
        _highlightMode = fetchChoiceParam(kHighlightMode);
        _colorTemperature = fetchIntParam(kColorTemperature);
        _cameraWhiteBalance = fetchBooleanParam(kCameraWhiteBalance);
        _timeRange = fetchInt2DParam(kTimeRange);
        if (_mlvfilename_param->getValue().empty() == false) {
            setMlvFile(_mlvfilename_param->getValue());
        }
        _pluginPath = getPluginFilePath();
        std::string focusPixelMap = _pluginPath + "/fpm";
        strcpy(FOCUSPIXELMAPFILE, focusPixelMap.c_str());
        pthread_mutex_init(&_mlv_mutex, NULL);
    }

    ~CMSMLVReaderPlugin()
    {
        pthread_mutex_destroy(&_mlv_mutex);
    }

private:
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
    std::string _mlvfilename;
    OFX::StringParam* _mlvfilename_param;
    OFX::ChoiceParam* _colorSpaceFormat;
    OFX::ChoiceParam* _debayerType;
    OFX::ChoiceParam* _highlightMode;
    OFX::IntParam* _colorTemperature;
    OFX::Int2DParam* _timeRange;
    OFX::BooleanParam* _cameraWhiteBalance;
    pthread_mutex_t _mlv_mutex;
    std::string _pluginPath;
    int _maxValue=0;
};

mDeclarePluginFactory(CMSMLVReaderPluginFactory, { OFX::ofxsThreadSuiteCheck(); }, {});

OFXS_NAMESPACE_ANONYMOUS_EXIT