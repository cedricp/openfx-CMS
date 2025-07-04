#pragma once

#include <cstdlib>
#include "ofxsProcessing.H"
#include "ofxsCoords.h"
#ifdef OFX_EXTENSIONS_NATRON
#include "ofxNatron.h"
#endif


#include <mlv_video.h>
#include <dng_convert.h>
#include <vector>

#include "OpenCLBase.h"

#include "mathutils.h"

#define kPluginName "MLVReader"
#define kPluginGrouping "MagicLantern"
#define kPluginDescription "Magic lantern MLV reader plugin"

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
#define kDarkframefilename "darkframeFilename"
#define kDarkFrameButon "darkFrameButton"
#define kDarkframeRange "darkframeRange"
#define kBlackLevel "blackLevel"
#define kWhiteLevel "whiteLevel"
#define kBpp "bpp"
#define kUseSpectralIdt "useSpectralIdt"
#define kResetLevels "resetLevels"
#define kCACorrectionThreshold "cacorrection_threshold"
#define kCACorrectionRadius "cacorrection_radius"
#define kGroupColorAberration "groupColorAberration"
#define kGroupWhiteBalance "groupWhiteBalance"

OFXS_NAMESPACE_ANONYMOUS_ENTER


#define OFX_COMPONENTS_OK(c) ((c) == OFX::ePixelComponentRGBA)

extern "C"
{
    extern char FOCUSPIXELMAP_DIRECTORY[256];
    extern int FOCUSPIXELMAP_OK;
}

void loadPlugin();

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class MLVReaderPlugin: public OpenCLBase
{
public:
    /** @brief ctor */
    MLVReaderPlugin(OfxImageEffectHandle handle) : OpenCLBase(handle)
    {
        _outputClip = fetchClip(kOfxImageEffectOutputClipName);
        _darkFrameButton = fetchPushButtonParam(kDarkFrameButon);
        _mlv_darkframefilename = fetchStringParam(kDarkframefilename);
        _darkframeRange = fetchInt2DParam(kDarkframeRange);
        _mlvfilename_param = fetchStringParam(kMLVfileParamter);
        _mlv_audiofilename = fetchStringParam(kAudioFilename);
        _audioExportButton = fetchPushButtonParam(kAudioExport);
        _outputColorSpace = fetchChoiceParam(kColorSpaceFormat);
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
        _blackLevel = fetchIntParam(kBlackLevel);
        _whiteLevel = fetchIntParam(kWhiteLevel);
        _bpp = fetchIntParam(kBpp);
        _useSpectralIdt = fetchBooleanParam(kUseSpectralIdt);
        _resetLevels = fetchBooleanParam(kResetLevels);
        _cacorrection_threshold = fetchDoubleParam(kCACorrectionThreshold);
        _cacorrection_radius = fetchIntParam(kCACorrectionRadius);

        _gThreadHost->multiThreadNumCPUs(&_numThreads);
        _gThreadHost->mutexCreate(&_videoMutex, 0);
        _gThreadHost->mutexCreate(&_idtMutex, 0);
        _pluginPath = getPluginFilePath();
        std::string focusPixelMap = _pluginPath + "/Contents/Resources/fpm";
        std::string debayer_program = _pluginPath + "/Contents/Resources/Shaders/debayer_ppg.cl";
        
        strcpy(FOCUSPIXELMAP_DIRECTORY, focusPixelMap.c_str());
        addProgram(debayer_program, "debayer_ppg");

        if (_mlvfilename_param->getValue().empty() == false) {
            setMlvFile(_mlvfilename_param->getValue(), false);
        }
    }

    ~MLVReaderPlugin()
    {
        for (Mlv_video* mlv : _mlv_video){
            if (mlv){
                delete mlv;
            }
        }
        _gThreadHost->mutexDestroy(_videoMutex);
        _gThreadHost->mutexDestroy(_idtMutex);
    }

private:
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;
    virtual void changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) OVERRIDE FINAL;
    virtual bool getTimeDomain(OfxRangeD& range) OVERRIDE FINAL;
    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
    virtual bool isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& identityClip, double& identityTime, int& view, std::string& plane) OVERRIDE;
    virtual bool isVideoStream(const std::string& ){return true;};
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    virtual void changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName) OVERRIDE FINAL;

    
    private:
    void renderCLTest(OFX::Image* destimg, int width, int height);
    void renderCPU(const OFX::RenderArguments &args, OFX::Image* dst, Mlv_video* mlv_video, int time, int height_img, int width_img);
    void renderCL(const OFX::RenderArguments &args, OFX::Image* destimg, Mlv_video* mlv_video, int time);

    Mlv_video* getMlv();
    void computeIDT();
    bool prepareSprectralSensIDT();
    void computeColorspaceMatrix(Matrix3x3f& out_matrix);
    void setMlvFile(std::string file, bool set = true);

    OfxMutexHandle _videoMutex, _idtMutex;

    unsigned int _numThreads;
    OFX::Clip* _outputClip;
    std::string _mlvfilename;
    OFX::StringParam* _mlvfilename_param;
    OFX::StringParam* _mlv_audiofilename;
    OFX::StringParam* _mlv_darkframefilename;
    OFX::PushButtonParam* _audioExportButton;
    OFX::PushButtonParam* _darkFrameButton;
    OFX::DoubleParam* _mlv_fps;
    OFX::ChoiceParam* _outputColorSpace;
    OFX::ChoiceParam* _debayerType;
    OFX::ChoiceParam* _highlightMode;
    OFX::ChoiceParam* _chromaSmooth;
    OFX::ChoiceParam* _dualIsoMode;
    OFX::ChoiceParam* _dualIsoAveragingMethod;
    OFX::IntParam* _colorTemperature;
    OFX::Int2DParam* _timeRange;
    OFX::Int2DParam* _darkframeRange;
    OFX::BooleanParam* _cameraWhiteBalance;
    OFX::BooleanParam* _fixFocusPixel;
    OFX::BooleanParam* _dualIsoFullresBlending;
    OFX::BooleanParam* _dualIsoAliasMap;
    OFX::BooleanParam* _useSpectralIdt;
    std::string _pluginPath;
    OFX::IntParam* _blackLevel;
    OFX::IntParam* _whiteLevel;
    OFX::IntParam* _bpp;
    OFX::BooleanParam* _resetLevels;
    OFX::DoubleParam* _cacorrection_threshold;
    OFX::IntParam* _cacorrection_radius;
    Matrix3x3f _idt;
    Vector3f _asShotNeutral;
    float _wbcompensation;
    int _maxValue=0;
    bool _idtDirty = true;
    bool _levelsDirty = true;

    std::vector<Mlv_video*> _mlv_video;
};

class MLVReaderPluginFactory : public OFX::PluginFactoryHelper<MLVReaderPluginFactory> { 
    public:
        MLVReaderPluginFactory(const std::string& id, unsigned int verMaj, unsigned int verMin)  :OFX::PluginFactoryHelper<MLVReaderPluginFactory>(id, verMaj, verMin)
        {}
        virtual void load() { loadPlugin(); }
        virtual void unload() {} ;
        virtual void describe(OFX::ImageEffectDescriptor &desc);
        virtual void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context);
        virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context);
};

OFXS_NAMESPACE_ANONYMOUS_EXIT

