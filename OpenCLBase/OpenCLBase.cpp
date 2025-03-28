#include "OpenCLBase.h"
#include <fstream>

static std::vector<cl::Device> g_cldevices;

OpenCLInstanceData::OpenCLInstanceData()
{
    g_cldevices.clear();
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    for (size_t i=0; i < platforms.size(); i++){
        std::vector<cl::Device> platformDevices;
        platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &platformDevices);
        for (size_t i=0; i < platformDevices.size(); i++) {
            g_cldevices.push_back(platformDevices[i]);
        }
    }
}

OpenCLInstanceData g_openclInstanceData;


static int _nextpow2(const int n)
{
  int k = 1;
  while(k < n)
    k <<= 1;
  return k;
}

int opencl_get_max_work_item_sizes(cl::Device dev,
    std::vector<size_t>& sizes)
{
    int err;
    sizes = dev.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>(&err);
    return err;
}

int opencl_get_work_group_limits(cl::Device dev,
    std::vector<size_t>& sizes,
    size_t& workgroupsize,
    unsigned long& localmemsize)
{
    cl_int err;

    localmemsize = dev.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>(&err);

    if(err != CL_SUCCESS) return err;

    workgroupsize = dev.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>(&err);

    if(err != CL_SUCCESS) return err;

    return opencl_get_max_work_item_sizes(dev, sizes);
}

int opencl_get_kernel_work_group_size(cl::Device dev,
    cl::Kernel ker,
    size_t& kernelworkgroupsize)
{
    cl_int err;
    kernelworkgroupsize = ker.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(dev, &err);
    return err;
}

int openCLGetLocalBufferOpt(cl::Device dev,
    cl::Kernel ker,
    OpenCLLocalBufferStruct *factors)
{
    std::vector<size_t> maxsizes(3);     // the maximum dimensions for a work group
    size_t workgroupsize = 0;       // the maximum number of items in a work group
    unsigned long localmemsize = 0; // the maximum amount of local memory we can use
    size_t kernelworkgroupsize = 0; // the maximum amount of items in
        // work group for this kernel

    int *blocksizex = &factors->sizex;
    int *blocksizey = &factors->sizey;

    // initial values must be supplied in sizex and sizey.
    // we make sure that these are a power of 2 and lie within reasonable limits.
    *blocksizex = CLAMP(_nextpow2(*blocksizex), 1, 1 << 16);
    *blocksizey = CLAMP(_nextpow2(*blocksizey), 1, 1 << 16);

    if(opencl_get_work_group_limits(dev, maxsizes, workgroupsize, localmemsize) == CL_SUCCESS
                                    && opencl_get_kernel_work_group_size
                                    (dev, ker, kernelworkgroupsize) == CL_SUCCESS)
    {
        while(maxsizes[0] < *blocksizex
            || maxsizes[1] < *blocksizey
            || localmemsize < ((factors->xfactor * (*blocksizex) + factors->xoffset) *
            (factors->yfactor * (*blocksizey) + factors->yoffset))
            * factors->cellsize + factors->overhead
            || workgroupsize < (size_t)(*blocksizex) * (*blocksizey)
            || kernelworkgroupsize < (size_t)(*blocksizex) * (*blocksizey))
        {
            if(*blocksizex == 1 && *blocksizey == 1)
            {
            printf(
            "[openCLGetLocalBufferOpt] no valid resource limits for curent device");
            return FALSE;
        }

        if(*blocksizex > *blocksizey)
        *blocksizex >>= 1;
        else
        *blocksizey >>= 1;
        }
    }
    else
    {
        printf(
        "can not identify"
        " resource limits for current device");
        return FALSE;
    }

    return TRUE;
}

void OpenCLBase::setupOpenCL()
{
    std::vector<std::pair<std::string, std::string>> paths = _program_paths;

    _programs.clear();
    _program_paths.clear();

    for (auto prog : paths){
        addProgram(prog.first, prog.second);
    }
}

bool OpenCLBase::addProgram(std::string program_path, std::string program_name)
{
    if (g_cldevices.empty()){
        return false;
    }

    std::ifstream programfile;
    programfile.open(program_path.c_str());
    std::ostringstream programtext;
    programtext << programfile.rdbuf();
    programfile.close();

    if (programtext.str().empty()){
        setPersistentMessage(OFX::Message::eMessageError, "", "Failed to load OpenCL program");
        return false;
    }

    clearPersistentMessage();

    int cl_dev = _openCLDevices->getValue();
    _current_cldevice = g_cldevices[cl_dev];

    cl::Platform platform(_current_cldevice.getInfo<CL_DEVICE_PLATFORM>());
    _current_clcontext = cl::Context(_current_cldevice);

    cl_int err;
    cl::Program prog(_current_clcontext, programtext.str(), true, &err);
    if (err != CL_SUCCESS){
        std::string errlog = prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(_current_cldevice);
        printf("OpenCL Error :\n%s\n", errlog.c_str());
        setPersistentMessage(OFX::Message::eMessageError, "", "Failed to create program");
        return false;
    }

    _programs[program_name] = prog;
    _program_paths.push_back(std::make_pair(program_path, program_name));

    clearPersistentMessage();

    return true;
}

void OpenCLBase::describeInContextCL(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context, OFX::PageParamDescriptor *page)
{
    { 
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kUseOpenCL);
        param->setLabel("OpenCL acceleration");
        param->setDefault(0);
        if (page)
        {
            page->addChild(*param);
        }
    }

    { 
        // linear, sRGB, Adobe, Wide, ProPhoto, XYZ, ACES, DCI-P3, Rec. 2020
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kOpenCLDevice);
        param->setLabel("OpenCL device");
        for (auto cldev : g_cldevices){
            param->appendOption(cldev.getInfo<CL_DEVICE_NAME>(), "", cldev.getInfo<CL_DEVICE_NAME>());
        }
        param->setDefault(0);
        param->setEnabled(false);
        if (page)
        {
            page->addChild(*param);
        }
    }
}

bool OpenCLBase::changedParamCL(OpenCLBase* instance, const OFX::InstanceChangedArgs& args, const std::string& paramName)
{
    bool use_opencl = instance->getUseOpenCL();

    if (paramName == kUseOpenCL || paramName == kOpenCLDevice){
        instance->_openCLDevices->setEnabled(use_opencl);
        if (use_opencl){
            instance->setupOpenCL();
        }
        return true;
    }
    return false;
}