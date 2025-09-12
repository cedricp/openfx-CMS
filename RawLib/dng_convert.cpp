#include "dng_convert.h"
#include <sys/stat.h>
#include <algorithm>
#include <libraw.h>

extern "C"{
	#include <dng/dng.h>
	#include <dng/dng_tag_values.h>
}

struct Dng_processor::dngc_impl{
	LibRaw* libraw;
	libraw_processed_image_t* _image;
};

Dng_processor::Dng_processor() : _buffer(NULL)
{
	_imp = new dngc_impl;
	_imp->libraw = new LibRaw;
	_imp->_image = 0;
	_w = _h = 0;
}

Dng_processor::~Dng_processor()
{
	free_buffer();
	//if (_filebuffer) free(_filebuffer);
	delete _imp->libraw;
	delete _imp;
}

void Dng_processor::unpack(uint8_t* buffer, size_t buffersize)
{
	int obret = _imp->libraw->open_buffer(buffer, buffersize);
	if (obret != LIBRAW_SUCCESS){
		printf("Open buffer error\n");
	}

	if(_imp->libraw->unpack() != LIBRAW_SUCCESS){
		printf("Unpack error\n");
	}
}

bool Dng_processor::load_dng(std::string filename)
{
	free_buffer();
	
	FILE* f = fopen(filename.c_str(), "rb");
	if (!f){
		printf("Dng_processor::load_dng : Cannot open file %s\n", filename.c_str());
		return false;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);
	unsigned char* buffer = (unsigned char*)malloc(size);
    size_t bytesRead = fread(buffer, 1, size, f);
	if (bytesRead != (size_t)size){
		printf("Dng_processor::load_dng : Cannot read file %s\n", filename.c_str());
		fclose(f);
		free(buffer);
		buffer = nullptr;
		return false;
	}

	get_processed_image(buffer, size);
	free(buffer);
	fclose(f);
	return true;
}

uint16_t* Dng_processor::get_processed_image(uint8_t* buffer, size_t buffersize)
{
	//_imp->libraw->recycle();

	/*
	# - Debayer method
	--+----------------------
	0 - linear interpolation
	1 - VNG interpolation
	2 - PPG interpolation
	3 - AHD interpolation
	4 - DCB interpolation
	*/

	// output_color -> linear, sRGB, Adobe, Wide, ProPhoto, XYZ, ACES, DCI-P3, Rec. 2020
	_imp->libraw->imgdata.params.output_color =  0; // Raw RGB
	_imp->libraw->imgdata.params.output_bps = 16;
	_imp->libraw->imgdata.params.gamm[0] = 1.0;
	_imp->libraw->imgdata.params.gamm[1] = 1.0;
	// See debayer method above
	_imp->libraw->imgdata.params.user_qual = _interpolation_mode < 5 ? _interpolation_mode : _interpolation_mode + 6;
	_imp->libraw->imgdata.params.use_camera_matrix = 0;
	_imp->libraw->imgdata.params.use_auto_wb = 0;
	// threshold-> Parameter for noise reduction through wavelet denoising.
	_imp->libraw->imgdata.params.threshold = 0.; 
	_imp->libraw->imgdata.params.bright = 1.;
	_imp->libraw->imgdata.params.no_auto_bright = 1.;
	_imp->libraw->imgdata.params.half_size = 0;
	_imp->libraw->imgdata.params.no_interpolation= 0;
	// Highlight mode is highlight reconstruction
	_imp->libraw->imgdata.params.highlight = _highlight_mode;
	_imp->libraw->imgdata.params.no_auto_scale = 1;

	_imp->libraw->imgdata.params.user_mul[0] = 1.;
	_imp->libraw->imgdata.params.user_mul[1] = 1.;
	_imp->libraw->imgdata.params.user_mul[2] = 1.;
	_imp->libraw->imgdata.params.user_mul[3] = 1.;

	unpack(buffer, buffersize);
	if (!buffer){
		printf("Dng_processor::get_processed_image : Nothing to unpack\n");
		return NULL;
	}

	int err;
	err = _imp->libraw->dcraw_process();
	if(err!= LIBRAW_SUCCESS){
		printf("dcraw process image error\n");
		return NULL;
	}

	_imp->_image = _imp->libraw->dcraw_make_mem_image(&err);
	
	if (err != LIBRAW_SUCCESS){
		printf("make mem image error\n");
		_imp->_image = nullptr;
		return NULL;
	}

	_w = _imp->_image->width;
	_h = _imp->_image->height;
	_bpp = _imp->_image->bits;
	_blacklevel = _imp->libraw->imgdata.color.dng_levels.dng_black;
	_whitelevel = _imp->libraw->imgdata.color.dng_levels.dng_whitelevel[0];

	_asShotNeutral = Vector3f(1./_imp->libraw->imgdata.color.dng_levels.asshotneutral[0],
								1./_imp->libraw->imgdata.color.dng_levels.asshotneutral[1],
								1./_imp->libraw->imgdata.color.dng_levels.asshotneutral[2]);

	_matrix1 = Matrix3x3f(_imp->libraw->imgdata.color.dng_color[0].colormatrix[0][0], _imp->libraw->imgdata.color.dng_color[0].colormatrix[0][1], _imp->libraw->imgdata.color.dng_color[0].colormatrix[0][2],
							_imp->libraw->imgdata.color.dng_color[0].colormatrix[1][0], _imp->libraw->imgdata.color.dng_color[0].colormatrix[1][1], _imp->libraw->imgdata.color.dng_color[0].colormatrix[1][2],
							_imp->libraw->imgdata.color.dng_color[0].colormatrix[2][0], _imp->libraw->imgdata.color.dng_color[0].colormatrix[2][1], _imp->libraw->imgdata.color.dng_color[0].colormatrix[2][2]);

	_matrix2 = Matrix3x3f(_imp->libraw->imgdata.color.dng_color[1].colormatrix[0][0], _imp->libraw->imgdata.color.dng_color[1].colormatrix[0][1], _imp->libraw->imgdata.color.dng_color[1].colormatrix[0][2],
							_imp->libraw->imgdata.color.dng_color[1].colormatrix[1][0], _imp->libraw->imgdata.color.dng_color[1].colormatrix[1][1], _imp->libraw->imgdata.color.dng_color[1].colormatrix[1][2],
							_imp->libraw->imgdata.color.dng_color[1].colormatrix[2][0], _imp->libraw->imgdata.color.dng_color[1].colormatrix[2][1], _imp->libraw->imgdata.color.dng_color[1].colormatrix[2][2]);

	_baseExpo = _imp->libraw->imgdata.color.dng_levels.baseline_exposure;
	_calibrate_expo[0] =  _imp->libraw->imgdata.color.dng_color[0].illuminant;
	_calibrate_expo[1] =  _imp->libraw->imgdata.color.dng_color[1].illuminant;

	_cam_mult[0] = _imp->libraw->imgdata.color.cam_mul[0];
	_cam_mult[1] = _imp->libraw->imgdata.color.cam_mul[1];
	_cam_mult[2] = _imp->libraw->imgdata.color.cam_mul[2];

	_camera_make = _imp->libraw->imgdata.idata.make;
	_camera_model = _imp->libraw->imgdata.idata.model;

	_init = true;

	return (uint16_t*)&_imp->_image->data;
}

uint16_t* Dng_processor::get_processed_filebuffer()
{
	if (_imp->_image) return (uint16_t*)&_imp->_image->data;
	return nullptr;
}

void Dng_processor::free_buffer()
{
	if (_imp->libraw){
		free(_imp->_image);
		_imp->_image = nullptr;
	}
}

void Dng_processor::kelvin_green_to_multipliers(double temperature, double green, double chanMulArray[3])
{
    float pre_mul[4], rgb_cam[3][4];
    double cam_xyz[4][3];
    double rgbWB[3];
    double cam_rgb[3][3];
    double rgb_cam_transpose[4][3];
    int c, cc, i, j;

    for (i = 0; i < 9; i++)
    {
        cam_xyz[i/3][i%3] = _matrix2[i/3][i%3] * 1000000.;
    }
    
    for (i = 9; i < 12; i++)
    {
        cam_xyz[i/3][i%3] = 0;
    }
    
    cam_xyz_coeff (cam_xyz, pre_mul, rgb_cam);
    
    for (i = 0; i < 4; i++) for (j = 0; j < 3; j++)
    {
        rgb_cam_transpose[i][j] = rgb_cam[j][i];
    }
    
    pseudoinverse(rgb_cam_transpose, cam_rgb, 3);
    
    temperature_to_RGB(temperature, rgbWB);
    rgbWB[1] = rgbWB[1] / green;
    
    for (c = 0; c < 3; c++)
    {
        double chanMulInv = 0;
        for (cc = 0; cc < 3; cc++)
            chanMulInv += 1 / pre_mul[c] * cam_rgb[c][cc] * rgbWB[cc];
        chanMulArray[c] = 1 / chanMulInv;
    }
    
    /* normalize green multiplier */
    chanMulArray[0] /= chanMulArray[1];
    chanMulArray[2] /= chanMulArray[1];
    chanMulArray[1] = 1;
}

void Dng_processor::get_white_balance_coeffs(int temperature, float coeffs[3], bool cam_wb)
{
	if (!cam_wb){
		int32_t wbal[6];
		mlv_wbal_hdr_t  wbobj;
		wbobj.kelvin = temperature;
		wbobj.wb_mode = WB_KELVIN;
		get_white_balance(wbobj, wbal);
		coeffs[0] = float(wbal[1]) / 1000000.f;
		coeffs[1] = float(wbal[3]) / 1000000.f;
		coeffs[2] = float(wbal[5]) / 1000000.f;
	} else {
		coeffs[0] = _imp->libraw->imgdata.color.cam_mul[0];
		coeffs[1] = _imp->libraw->imgdata.color.cam_mul[1];
		coeffs[2] = _imp->libraw->imgdata.color.cam_mul[2];
	}
}

void Dng_processor::get_white_balance(mlv_wbal_hdr_t wbal_hdr, int32_t *wbal)
{
	double green = 1.0;
	double kelvin = wbal_hdr.kelvin;

	double chanMulArray[3];
	kelvin_green_to_multipliers(kelvin, green, chanMulArray);
	wbal[0] = 1000000; wbal[1] = (int32_t)(chanMulArray[0] * 1000000);
	wbal[2] = 1000000; wbal[3] = (int32_t)(chanMulArray[1] * 1000000);
	wbal[4] = 1000000; wbal[5] = (int32_t)(chanMulArray[2] * 1000000);
}