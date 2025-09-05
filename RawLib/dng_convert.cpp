#include "dng_convert.h"
#include <sys/stat.h>
#include <algorithm>
#include <libraw.h>
extern "C"{
	#include <dng/dng.h>
}

struct Dng_processor::dngc_impl{
	LibRaw* libraw;
	libraw_processed_image_t* _image;
};

Dng_processor::Dng_processor() : _buffer(NULL), _filebuffer(NULL)
{
	_imp = new dngc_impl;
	_imp->libraw = new LibRaw;
	_imp->_image = 0;
	_w = _h = 0;
}

Dng_processor::~Dng_processor()
{
	free_buffer();
	free(_filebuffer);
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

void Dng_processor::load_dng(std::string filename)
{
	if (_filebuffer){
		free(_filebuffer);
		_filebuffer = NULL;
	}
	
	FILE* f = fopen(filename.c_str(), "rb");
	if (!f){
		printf("Dng_processor::load_dng : Cannot open file %s\n", filename.c_str());
		return;
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
		return;
	}

	_filebuffer = get_processed_image(buffer, size);
	free(buffer);
	fclose(f);
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
		return NULL;
	}

	_w = _imp->_image->width;
	_h = _imp->_image->height;
	_bpp = _imp->_image->bits;
	_blacklevel = _imp->libraw->imgdata.color.dng_levels.dng_black;
	_whitelevel = _imp->libraw->imgdata.color.dng_levels.dng_whitelevel[0];

	_cam2xyz = Matrix3x3f(_imp->libraw->imgdata.color.cam_xyz[0][0], _imp->libraw->imgdata.color.cam_xyz[1][0], _imp->libraw->imgdata.color.cam_xyz[2][0],
						  	_imp->libraw->imgdata.color.cam_xyz[0][1], _imp->libraw->imgdata.color.cam_xyz[1][1], _imp->libraw->imgdata.color.cam_xyz[2][1],
						  	_imp->libraw->imgdata.color.cam_xyz[0][2], _imp->libraw->imgdata.color.cam_xyz[1][2], _imp->libraw->imgdata.color.cam_xyz[2][2]);

	_asShotNeutral = Vector3f(1.f/_imp->libraw->imgdata.color.dng_levels.asshotneutral[0],
								1.f/_imp->libraw->imgdata.color.dng_levels.asshotneutral[1],
								1.f/_imp->libraw->imgdata.color.dng_levels.asshotneutral[2]);

	_matrix1 = Matrix3x3f(_imp->libraw->imgdata.color.dng_color[0].colormatrix[0][0], _imp->libraw->imgdata.color.dng_color[0].colormatrix[1][0], _imp->libraw->imgdata.color.dng_color[0].colormatrix[2][0],
							_imp->libraw->imgdata.color.dng_color[0].colormatrix[0][1], _imp->libraw->imgdata.color.dng_color[0].colormatrix[1][1], _imp->libraw->imgdata.color.dng_color[0].colormatrix[2][1],
							_imp->libraw->imgdata.color.dng_color[0].colormatrix[0][2], _imp->libraw->imgdata.color.dng_color[0].colormatrix[1][2], _imp->libraw->imgdata.color.dng_color[0].colormatrix[2][2]);

	_matrix2 = Matrix3x3f(_imp->libraw->imgdata.color.dng_color[1].colormatrix[0][0], _imp->libraw->imgdata.color.dng_color[1].colormatrix[1][0], _imp->libraw->imgdata.color.dng_color[1].colormatrix[2][0],
							_imp->libraw->imgdata.color.dng_color[1].colormatrix[0][1], _imp->libraw->imgdata.color.dng_color[1].colormatrix[1][1], _imp->libraw->imgdata.color.dng_color[1].colormatrix[2][1],
							_imp->libraw->imgdata.color.dng_color[1].colormatrix[0][2], _imp->libraw->imgdata.color.dng_color[1].colormatrix[1][2], _imp->libraw->imgdata.color.dng_color[1].colormatrix[2][2]);

	_baseExpo = _imp->libraw->imgdata.color.dng_levels.baseline_exposure;
	_calibrate_expo[0] =  _imp->libraw->imgdata.color.dng_color[0].illuminant;
	_calibrate_expo[1] =  _imp->libraw->imgdata.color.dng_color[1].illuminant;
	return (uint16_t*)&_imp->_image->data;
}

void Dng_processor::free_buffer()
{
	if (_imp->libraw){
		free(_imp->_image);
		_imp->_image = 0;
	}
}

