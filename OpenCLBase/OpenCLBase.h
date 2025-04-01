#pragma once

#define CL_HPP_ENABLE_SIZE_T_COMPATIBILITY 1
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION 120
#include <CL/opencl.hpp>
#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "ofxsThreadSuite.h"
#include <map>

#define kUseOpenCL "UseOpenCL"
#define kOpenCLDevice "OpenCLDevice"


class OpenCLInstanceData
{
    public:
    OpenCLInstanceData();
};

class OpenCLBase: public OFX::ImageEffect
{
public:
    /** @brief ctor */
    OpenCLBase(OfxImageEffectHandle handle) : OFX::ImageEffect(handle)
    {
        _openCLDevices = fetchChoiceParam(kOpenCLDevice);
        _useOpenCL = fetchBooleanParam(kUseOpenCL);
        _gThreadHost->mutexCreate(&_OclMutex, 0);
    }

    virtual ~OpenCLBase()
    {
        _gThreadHost->mutexDestroy(_OclMutex);
    }

    bool addProgram(std::string program_path, std::string program_name);

    static void describeInContextCL(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context, OFX::PageParamDescriptor *page);
    static bool changedParamCL(OpenCLBase* instance, const OFX::InstanceChangedArgs& args, const std::string& paramName);
    bool getUseOpenCL(){return _useOpenCL->getValue();}
    cl::Program getProgram(std::string program_name){return _programs[program_name];}
protected:
    void setupOpenCL();
    cl::Device& getCurrentCLDevice(){return _current_cldevice;}
    cl::Context& getCurrentCLContext(){return _current_clcontext;}
    OfxMultiThreadSuiteV1 *_gThreadHost = 0;

private:
    OfxMutexHandle _OclMutex;

    std::map<std::string, cl::Program> _programs;
    std::vector<std::pair<std::string, std::string>> _program_paths;
    OFX::ChoiceParam* _openCLDevices;
    OFX::BooleanParam* _useOpenCL;

    cl::Device _current_cldevice;
    cl::Context _current_clcontext;
};

struct OpenCLLocalBufferStruct
{
  const int xoffset;
  const int xfactor;
  const int yoffset;
  const int yfactor;
  const size_t cellsize;
  const size_t overhead;
  int sizex;  // initial value and final values after optimization
  int sizey;  // initial value and final values after optimization
};


#define CLAMP(A, L, H) ((A) > (L) ? ((A) < (H) ? (A) : (H)) : (L))
#define ROUNDUP(a, n) ((a) % (n) == 0 ? (a) : ((a) / (n)+1) * (n))

int openCLGetLocalBufferOpt(cl::Device dev,
    cl::Kernel ker,
    OpenCLLocalBufferStruct *factors);