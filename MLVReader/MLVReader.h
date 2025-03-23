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

#define kPluginName "MLVReader"
#define kPluginGrouping "MagicLantern"
#define kPluginDescription                                                                                                                                                                                                                                             \
"Magic lantern MLV reader plugin"

#define kPluginIdentifier "net.sf.openfx.MLVReader"
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
#define kFrameRange "Framerange"
#define kChromaSmooth "ChromaSmooth"
#define kFixFocusPixel "FixFocusPixel"
#define kMlvFps "MlvFps"
#define kDualIso "MlvDualIso"
#define kDualIsoAliasMap "dualIsoAliasMap"
#define kDualIsoFullresBlending "dualIsoFullResBlending"
#define kDualIsoAveragingMethod "dualIsoAveragingMethod"
#define kAudioFilename "audioFilename"
#define kAudioExport "audioExport"
#include <vector>


OFXS_NAMESPACE_ANONYMOUS_ENTER


#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGB)

extern "C"
{
    extern char FOCUSPIXELMAPFILE[256];
}
////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class MLVReaderPlugin: public OFX::ImageEffect
{
public:
    /** @brief ctor */
    MLVReaderPlugin(OfxImageEffectHandle handle) : OFX::ImageEffect(handle)
    {
        _mlvfilename_param = fetchStringParam(kMLVfileParamter);
        _mlv_audiofilename = fetchStringParam(kAudioFilename);
        _audioExportButton = fetchPushButtonParam(kAudioExport);
        _outputClip = fetchClip(kOfxImageEffectOutputClipName);
        _colorSpaceFormat = fetchChoiceParam(kColorSpaceFormat);
        _debayerType = fetchChoiceParam(kDebayerType);
        _highlightMode = fetchChoiceParam(kHighlightMode);
        _chromaSmooth = fetchChoiceParam(kChromaSmooth);
        _colorTemperature = fetchIntParam(kColorTemperature);
        _cameraWhiteBalance = fetchBooleanParam(kCameraWhiteBalance);
        _timeRange = fetchInt2DParam(kFrameRange);
        _fixFocusPixel = fetchBooleanParam(kFixFocusPixel);
        _mlv_fps = fetchDoubleParam(kMlvFps);
        _dualIsoMode = fetchChoiceParam(kDualIso);
        _dualIsoAliasMap = fetchBooleanParam(kDualIsoAliasMap);
        _dualIsoFullresBlending = fetchBooleanParam(kDualIsoFullresBlending);
        _dualIsoAveragingMethod = fetchChoiceParam(kDualIsoAveragingMethod);
        _gThreadHost = (OfxMultiThreadSuiteV1 *) OFX::fetchSuite(kOfxMultiThreadSuite, 1);
        _gThreadHost->multiThreadNumCPUs(&_numThreads);
        _gThreadHost->mutexCreate(&_videoMutex, 0);
        if (_mlvfilename_param->getValue().empty() == false) {
            setMlvFile(_mlvfilename_param->getValue());
        }
        _pluginPath = getPluginFilePath();
        std::string focusPixelMap = _pluginPath + "/Contents/fpm";
        strcpy(FOCUSPIXELMAPFILE, focusPixelMap.c_str());
    }

    ~MLVReaderPlugin()
    {
        for (Mlv_video* mlv : _mlv_video){
            if (mlv){
                delete mlv;
            }
        }
        _gThreadHost->mutexDestroy(_videoMutex);
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
    OfxMultiThreadSuiteV1 *_gThreadHost = 0;
    OfxMutexHandle _videoMutex;

    unsigned int _numThreads;
    OFX::Clip* _outputClip;
    std::string _mlvfilename;
    OFX::StringParam* _mlvfilename_param;
    OFX::StringParam* _mlv_audiofilename;
    OFX::PushButtonParam* _audioExportButton;
    OFX::DoubleParam* _mlv_fps;
    OFX::ChoiceParam* _colorSpaceFormat;
    OFX::ChoiceParam* _debayerType;
    OFX::ChoiceParam* _highlightMode;
    OFX::ChoiceParam* _chromaSmooth;
    OFX::ChoiceParam* _dualIsoMode;
    OFX::ChoiceParam* _dualIsoAveragingMethod;
    OFX::IntParam* _colorTemperature;
    OFX::Int2DParam* _timeRange;
    OFX::BooleanParam* _cameraWhiteBalance;
    OFX::BooleanParam* _fixFocusPixel;
    OFX::BooleanParam* _dualIsoFullresBlending;
    OFX::BooleanParam* _dualIsoAliasMap;
    std::string _pluginPath;
    int _maxValue=0;

    std::vector<Mlv_video*> _mlv_video;
};

mDeclarePluginFactory(MLVReaderPluginFactory, { OFX::ofxsThreadSuiteCheck(); }, {});

OFXS_NAMESPACE_ANONYMOUS_EXIT