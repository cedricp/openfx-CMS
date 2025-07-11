
extern "C"{
	#include "video_mlv.h"
	#include "dng/dng.h"
	#include "llrawproc/llrawproc.h"
	#include "audio_mlv.h"
	#include <camid/camera_id.h>
}
#include <string.h>
#include <algorithm>
#include "mlv_video.h"
#include <iostream>
#include "lens_id.h"
#include "dng_convert.h"

struct mlv_imp
{
	mlvObject_t* mlv_object = NULL;
	dngObject_t* dng_object = NULL;
	std::string mlvfilename;
};

std::string get_map_name(mlvObject_t* mvl_object)
{
    char name[1024];
    snprintf(name, 1024, "%x_%ix%i.fpm", mvl_object->IDNT.cameraModel, mvl_object->RAWI.raw_info.width, mvl_object->RAWI.raw_info.height);
    return name;
}


Mlv_video::Mlv_video(std::string filename)
{
	_shared = false;
	_imp = new mlv_imp;
	_imp->mlvfilename = filename;

	int err;
	char err_mess[512];

	_imp->mlv_object = initMlvObjectWithClip(filename.c_str(), &err, err_mess);
	_imp->dng_object = NULL;

	
	if (err){
		std::cout << "MLV open problem : " << err_mess << std::endl;
		_valid = false;
		return;
	}

	_valid = true;

	int par[4] = {1,1,1,1};
	_imp->dng_object = initDngObject(_imp->mlv_object, UNCOMPRESSED_RAW, getMlvFramerateOrig(_imp->mlv_object), par, -1, -1);
}

Mlv_video::Mlv_video(const Mlv_video& mlv)
{
	_shared = true;
	_imp = new mlv_imp;
	_imp->mlvfilename = mlv._imp->mlvfilename;

	_imp->mlv_object = (mlvObject_t*)malloc(sizeof(mlvObject_t));

	memcpy(_imp->mlv_object, mlv._imp->mlv_object, sizeof(mlvObject_t));
	initMlvFiles(_imp->mlvfilename.c_str(), _imp->mlv_object);

	_imp->dng_object = NULL;
	_valid = true;

	int par[4] = {1,1,1,1};
	_imp->dng_object = initDngObject(_imp->mlv_object, UNCOMPRESSED_RAW, getMlvFramerateOrig(_imp->mlv_object), par, -1, -1);
}

Mlv_video::~Mlv_video()
{
	freeMlvObject(_imp->mlv_object, _shared);
	freeDngObject(_imp->dng_object);
	delete _imp;
}

void* Mlv_video::get_mlv_object()
{
	return (void*)_imp->mlv_object;
}

uint32_t Mlv_video::get_dng_header_size()
{
	return _imp->dng_object->header_size;
}

uint16_t* Mlv_video::get_raw_image()
{
	return _imp->dng_object->image_buf;
}

int Mlv_video::get_camid()
{
	return _imp->mlv_object->IDNT.cameraModel;
}

int32_t* Mlv_video::get_camera_forward_matrix2()
{
	return camidGetForwardMatrix2(get_camid());
}

void Mlv_video::get_camera_forward_matrix2f(float matrix[9])
{
	int32_t* matrixi = camidGetForwardMatrix2(get_camid());
	for (int i = 0; i < 9; i++){
		matrix[i] = (float)matrixi[i*2] / 10000.f;
	}
}

void Mlv_video::get_camera_matrix2f(float matrix[9])
{
	int32_t* matrixi = camidGetColorMatrix2(get_camid());
	for (int i = 0; i < 9; i++){
		matrix[i] = (float)matrixi[i*2] / 10000.f;
	}
}

void Mlv_video::get_camera_matrix1f(float matrix[9])
{
	int32_t* matrixi = camidGetColorMatrix1(get_camid());
	for (int i = 0; i < 9; i++){
		matrix[i] = (float)matrixi[i*2] / 10000.f;
	}
}

void Mlv_video::get_camera_forward_matrix1f(float matrix[9])
{
	int32_t* matrixi = camidGetForwardMatrix1(get_camid());
	for (int i = 0; i < 9; i++){
		matrix[i] = (float)matrixi[i*2] / 10000.f;
	}
}

void Mlv_video::get_baseline_exposure(int32_t& min, int32_t& max)
{
	min = _imp->mlv_object->RAWI.raw_info.exposure_bias[0];
	max = _imp->mlv_object->RAWI.raw_info.exposure_bias[1];
	if (max == 0)
	{
		min = 0;
		max = 1;
	}
}

bool Mlv_video::generate_darkframe(const char* path, int frame_in, int frame_out)
{
	char error_message[256] = { 0 };
	FILE* mlv_file = fopen(path, "wb");
	mlvObject_t* video = _imp->mlv_object;

	printf("Averaging frames %d to %d from MLV file: %s\n", frame_in, frame_out, video->path);

 	uint64_t* avg_buf = (uint64_t *)calloc( video->RAWI.xRes * video->RAWI.yRes * sizeof( uint64_t ), 1 );

    mlv_vidf_hdr_t vidf_hdr = { 0 };

    uint32_t pixel_count = video->RAWI.xRes * video->RAWI.yRes;
    uint32_t frame_size_packed = (uint32_t)(pixel_count * video->RAWI.raw_info.bits_per_pixel / 8);
    uint32_t frame_size_unpacked = pixel_count * 2;
    uint32_t max_frame_number = frame_out - frame_in + 1;

	uint16_t * frame_buf_unpacked = (uint16_t*)calloc(frame_size_unpacked, 1);
	if(!frame_buf_unpacked)
	{
		sprintf(error_message, "Averaging: could not allocate memory for unpacked frame");
		printf("\n%s\n", error_message);
		free(avg_buf);
		return false;
	}

	/* for safety allocate max possible size buffer for VIDF block, calculated for 16bits per pixel */
	uint8_t * block_buf = (uint8_t*)calloc(sizeof(mlv_vidf_hdr_t) + frame_size_unpacked, 1);
	if(!block_buf)
	{
		sprintf(error_message, "Could not allocate memory for VIDF block");
		printf("\n%s\n", error_message);
		free(frame_buf_unpacked);
		free(avg_buf);
		return false;
	}
	/* for safety allocate max possible size buffer for image data, calculated for 16bits per pixel */
	uint8_t * frame_buf = (uint8_t*)calloc(frame_size_unpacked, 1);
	if(!frame_buf)
	{
		sprintf(error_message, "Could not allocate memory for VIDF frame");
		printf("\n%s\n", error_message);
		free(block_buf);
		return false;
	}

	saveMlvHeaders(video, mlv_file, 0, MLV_AVERAGED_FRAME, frame_in, frame_out, "1.0", error_message);

	for (int i = frame_in; i < frame_out; ++i){
    	int chunk = video->video_index[i].chunk_num;
		uint32_t frame_size = video->video_index[i].frame_size;
		uint64_t frame_offset = video->video_index[i].frame_offset;
		uint64_t block_offset = video->video_index[i].block_offset;
		/* read VIDF block header */
		fseek(video->file[chunk], block_offset, SEEK_SET);
		if(fread(&vidf_hdr, sizeof(mlv_vidf_hdr_t), 1, video->file[chunk]) != 1)
		{
			sprintf(error_message, "Could not read VIDF block header from:  %s", video->path);
			printf("\n%s\n", error_message);
			free(frame_buf);
			free(block_buf);
			free(frame_buf_unpacked);
			free(avg_buf);
			return false;
		}

		vidf_hdr.blockSize -= vidf_hdr.frameSpace;
		vidf_hdr.frameSpace = 0;


		/* read frame buffer */
		fseek(video->file[chunk], frame_offset, SEEK_SET);
		if(fread(frame_buf, frame_size, 1, video->file[chunk]) != 1)
		{
			sprintf(error_message, "Could not read VIDF image data from:  %s", video->path);
			printf("\n%s\n", error_message);
			free(frame_buf);
			free(block_buf);
			free(frame_buf_unpacked);
			free(avg_buf);
			return false;
		}

		{
			if(isMlvCompressed(video))
			{
				int ret = dng_decompress_image(frame_buf_unpacked, (uint16_t*)frame_buf, frame_size, video->RAWI.xRes, video->RAWI.yRes, video->RAWI.raw_info.bits_per_pixel);
				if(ret != 0)
				{
					sprintf(error_message, "Averaging: could not decompress frame:  LJ92_ERROR %u", ret);
					printf("\n%s\n", error_message);
					free(frame_buf_unpacked);
					free(frame_buf);
					free(block_buf);
					free(avg_buf);
					return false;
				}
			}
			else
			{
				dng_unpack_image_bits(frame_buf_unpacked, (uint16_t*)frame_buf, video->RAWI.xRes, video->RAWI.yRes, video->RAWI.raw_info.bits_per_pixel);
			}
			for(uint32_t i = 0; i < pixel_count; i++)
			{
				avg_buf[i] += frame_buf_unpacked[i];
			}

		}
	}

	for(uint32_t i = 0; i < pixel_count; i++)
	{
		frame_buf_unpacked[i] = avg_buf[i] / max_frame_number;
	}
	dng_pack_image_bits((uint16_t *)frame_buf, frame_buf_unpacked, video->RAWI.xRes, video->RAWI.yRes, video->RAWI.raw_info.bits_per_pixel, 0);

	vidf_hdr.frameNumber = max_frame_number;
	vidf_hdr.blockSize = sizeof(mlv_vidf_hdr_t) + frame_size_packed;
	memcpy(block_buf, &vidf_hdr, sizeof(mlv_vidf_hdr_t));
	memcpy((block_buf + sizeof(mlv_vidf_hdr_t)), frame_buf, frame_size_packed);

	if(fwrite(block_buf, vidf_hdr.blockSize, 1, mlv_file) != 1)
	{
		sprintf(error_message, "Could not write darkvideo frame");
	}
	fclose(mlv_file);

	free(frame_buf_unpacked);
	free(avg_buf);
	free(frame_buf);
	free(block_buf);
	return true;
}

void Mlv_video::destroy_darkframe_data()
{
	mlvObject_t* mlv = _imp->mlv_object;
	if (mlv->llrawproc->dark_frame_data != NULL)
	{
		free(mlv->llrawproc->dark_frame_data);
		mlv->llrawproc->dark_frame_data = NULL;
	}
}

void Mlv_video::write_audio(std::string path)
{
	mlvObject_t* mlv = _imp->mlv_object;
	const char* wave_path = path.c_str();
	readMlvAudioData(mlv);
	writeMlvAudioToWave(mlv, wave_path);
}

uint32_t Mlv_video::raw_resolution_x()
{
	return getMlvWidth(_imp->mlv_object);
}

uint32_t Mlv_video::raw_resolution_y()
{
	return getMlvHeight(_imp->mlv_object);
}

uint32_t Mlv_video::raw_black_level()
{
	return _imp->mlv_object->RAWI.raw_info.black_level;
}

uint32_t Mlv_video::raw_white_level()
{
	return _imp->mlv_object->RAWI.raw_info.white_level;
}

float Mlv_video::fps()
{
	return getMlvFramerateOrig(_imp->mlv_object);
}

uint32_t Mlv_video::black_level()
{
	return _imp->mlv_object->llrawproc->dng_black_level;
}

uint32_t Mlv_video::white_level()
{
	return _imp->mlv_object->llrawproc->dng_white_level;
}

void Mlv_video::low_level_process(RawInfo& ri)
{
	mlvObject_t mlvob = *_imp->mlv_object;

	int cs = 0;
	switch (ri.chroma_smooth){
        case 1:
        cs = 1;
        break;
        case 2:
        cs = 2;
        break;
        case 3:
        cs = 5;
        break;
        case 0:
        default:
        cs = 0;
    }

	
	llrpSetFixRawMode(&mlvob, 1);
	llrpSetChromaSmoothMode(&mlvob, cs);
	llrpResetDngBWLevels(&mlvob);

	llrpSetDualIsoMode(&mlvob, ri.dual_iso_mode);
	llrpSetDualIsoAliasMapMode(&mlvob, (int)ri.dualiso_aliasmap);
	llrpSetDualIsoFullResBlendingMode(&mlvob, (int)ri.dualiso_fullres_blending);
	llrpSetDualIsoInterpolationMethod(&mlvob, ri.dualisointerpolation);

	char error_msg[128];
	if (ri.darkframe_enable){
		// Avoid reloading the dark frame if it is already set
		if (!ri.darkframe_file.empty() && mlvob.llrawproc->dark_frame_filename != NULL && strcmp(ri.darkframe_file.c_str(), mlvob.llrawproc->dark_frame_filename) == 0 && mlvob.llrawproc->dark_frame_data != NULL){
			llrpSetDarkFrameMode(&mlvob, 1);
			ri.darkframe_ok = true;
		} else {
			if( llrpValidateExtDarkFrame(&mlvob, ri.darkframe_file.c_str(), error_msg) == 0 ){
				llrpSetDarkFrameMode(&mlvob, 1);
				llrpInitDarkFrameExtFileName(&mlvob, ri.darkframe_file.c_str());
				ri.darkframe_ok = true;
				ri.darkframe_error = error_msg;
			} else {
				llrpSetDarkFrameMode(&mlvob, 0);
				if (mlvob.llrawproc->dark_frame_data != NULL){
					free(mlvob.llrawproc->dark_frame_data);
					mlvob.llrawproc->dark_frame_data = NULL;
				}
				if(mlvob.llrawproc->dark_frame_filename) free(mlvob.llrawproc->dark_frame_filename);
    			mlvob.llrawproc->dark_frame_filename = NULL;
				ri.darkframe_ok = false;
				ri.darkframe_error.clear();
			}
		}
	} else {
		llrpSetDarkFrameMode(&mlvob, 0);
		if (mlvob.llrawproc->dark_frame_data != NULL){
			free(mlvob.llrawproc->dark_frame_data);
			mlvob.llrawproc->dark_frame_data = NULL;
		}
		ri.darkframe_error.clear();        
	}

	if (ri.fix_focuspixels){
		int focusDetect = llrpDetectFocusDotFixMode(&mlvob);
		if( focusDetect != 0 ){
			llrpSetFocusPixelMode(&mlvob, focusDetect);
		}
	} else {
		llrpSetFocusPixelMode(&mlvob, FP_OFF);
	}
}

uint16_t* Mlv_video::get_dng_buffer(uint32_t frame, int& dng_size, bool no_buffer)
{
	mlvObject_t mlvob = *_imp->mlv_object;
	
	if (frame >= mlvob.frames){
		frame = mlvob.frames - 1;
	}

	uint8_t *buffer = getDngFrameBuffer(&mlvob, _imp->dng_object, frame, no_buffer ? 1 : 0);
	
	size_t size = _imp->dng_object->image_size + _imp->dng_object->header_size;
	
	if (no_buffer){
		dng_size = 0;
		return nullptr;
	}

	dng_size = size;

	return (uint16_t*)buffer;
}

void Mlv_video::set_dng_raw_levels(int black, int white)
{
	mlvObject_t mlvob = *_imp->mlv_object;

	_imp->dng_object->black_level = black;
	_imp->dng_object->white_level = white;
}

uint16_t* Mlv_video::postprocecessed_raw_buffer()
{
	return _imp->dng_object->image_buf_unpacked;
}

std::string Mlv_video::camera_name()
{
	return std::string((const char*)_imp->mlv_object->IDNT.cameraName);
}

std::string Mlv_video::lens_name()
{
	std::string lensname = ((char*)_imp->mlv_object->LENS.lensName);
	return lensname;
}

std::string Mlv_video::lens_name_by_id()
{
	std::vector<std::string> lenses = get_lens_by_type(_imp->mlv_object->LENS.lensID);
	if (!lenses.empty()){
		return lenses[0];
	}
	return ((char*)_imp->mlv_object->LENS.lensName);;
}

float Mlv_video::focal_length()
{
	return _imp->mlv_object->LENS.focalLength;
}

float Mlv_video::focal_dist()
{
	return (float)_imp->mlv_object->LENS.focalDist / 100.0f;
}

void Mlv_video::sensor_resolulion(int& x, int& y)
{
	x = _imp->mlv_object->RAWC.sensor_res_x;
	y = _imp->mlv_object->RAWC.sensor_res_y;
}

float Mlv_video::aperture()
{
	return _imp->mlv_object->LENS.aperture / 100.0f;
}

uint32_t Mlv_video::frame_count()
{
	return _imp->mlv_object->frames;
}

float Mlv_video::crop_factor()
{
	return float(_imp->mlv_object->RAWC.sensor_crop) / 100.f;
}

float Mlv_video::final_crop_factor()
{
	const mlv_rawc_hdr_t& m = _imp->mlv_object->RAWC;
	float ratio = ((float)m.sensor_res_x / ((float)sampling_factor_x() * (float)raw_resolution_x())) * crop_factor();
	if(ratio < crop_factor()){
		ratio = crop_factor();
	}
	return ratio;
}

uint32_t Mlv_video::iso()
{
	return _imp->mlv_object->EXPO.isoValue;
}

int Mlv_video::bpp()
{
	return _imp->mlv_object->lossless_bpp;//raw_info.bits_per_pixel;
}

uint32_t Mlv_video::shutter_speed()
{
	return _imp->mlv_object->EXPO.shutterValue / uint64_t(1000);
}

int Mlv_video::pixel_binning_x()
{
	return _imp->mlv_object->RAWC.binning_x;
}

int Mlv_video::pixel_binning_y()
{
	return _imp->mlv_object->RAWC.binning_y;
}

int Mlv_video::sampling_factor_x()
{
	return _imp->mlv_object->RAWC.binning_x + _imp->mlv_object->RAWC.skipping_x;
}

int Mlv_video::sampling_factor_y()
{
	return _imp->mlv_object->RAWC.binning_y + _imp->mlv_object->RAWC.skipping_y;
}

std::string Mlv_video::get_camera_make()
{
	std::string make = (const char*)_imp->mlv_object->IDNT.cameraName;
	return make.substr(0, make.find(" "));
}

std::string Mlv_video::get_camera_model()
{
	std::string make = (const char*)_imp->mlv_object->IDNT.cameraName;
	return make.substr(make.find(" ")+1, std::string::npos);
}

void Mlv_video::get_white_balance_coeffs(int temperature, float coeffs[3], float &compensation,bool cam_wb)
{
	int32_t wbal[6];
	mlv_wbal_hdr_t  wbobj = _imp->mlv_object->WBAL;
	if (!cam_wb){
		wbobj.wb_mode = WB_KELVIN;
		wbobj.kelvin = temperature;
	}
	::get_white_balance(wbobj, wbal, get_camid());
	coeffs[0] = float(wbal[1]) / 1000000.f;
	coeffs[1] = float(wbal[3]) / 1000000.f;
	coeffs[2] = float(wbal[5]) / 1000000.f;
	compensation = *std::max_element(coeffs, coeffs+3) / *std::min_element(coeffs, coeffs+3);
}