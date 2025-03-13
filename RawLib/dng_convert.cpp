#include "dng_convert.h"
#include "idt/dng_idt.h"
#include <libraw.h>
#include <sys/stat.h>
#include <algorithm>

struct Dng_processor::dngc_impl{
	LibRaw* libraw;
};

Dng_processor::Dng_processor() : _buffer(NULL)
{
	_imp = new dngc_impl;
	_imp->libraw = new LibRaw;
	_w = _h = 0;
}

Dng_processor::~Dng_processor()
{
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

uint16_t* Dng_processor::get_processed_image(uint8_t* buffer, size_t buffersize)
{
	libraw_processed_image_t* image;
	_imp->libraw->recycle();

	/*
	# - Debayer method
	--+----------------------
	0 - linear interpolation
	1 - VNG interpolation
	2 - PPG interpolation
	3 - AHD interpolation
	4 - DCB interpolation
	11 - DHT intepolation
	12 - Modified AHD intepolation (by Anton Petrusevich)
	*/

	unpack(buffer, buffersize);
	if (!buffer){
		printf("Dng_processor::get_processed_image : Nothing to unpack\n");
		return NULL;
	}
	
	// XYZ colorspace
	_imp->libraw->imgdata.params.use_auto_wb = 0;
	// output_color -> raw, sRGB, Adobe, Wide, ProPhoto, XYZ, ACES, DCI-P3, Rec. 2020
	_imp->libraw->imgdata.params.output_color = 5;
	_imp->libraw->imgdata.params.output_bps = 16;
	_imp->libraw->imgdata.params.gamm[0] = 1.0;
	_imp->libraw->imgdata.params.gamm[1] = 1.0;
	// See debayer method above
	_imp->libraw->imgdata.params.user_qual = _interpolation_mode;
	_imp->libraw->imgdata.params.use_camera_matrix = 1;
	_imp->libraw->imgdata.params.use_camera_wb = _camera_wb;
	// threshold-> Parameter for noise reduction through wavelet denoising.
	_imp->libraw->imgdata.params.threshold = 1.; 
	// _imp->libraw->imgdata.params.user_mul[0] = _imp->libraw->imgdata.color.cam_mul[0];
	// _imp->libraw->imgdata.params.user_mul[1] = _imp->libraw->imgdata.color.cam_mul[1];
	// _imp->libraw->imgdata.params.user_mul[2] = _imp->libraw->imgdata.color.cam_mul[2];
	// _imp->libraw->imgdata.params.user_mul[3] = _imp->libraw->imgdata.color.cam_mul[3];
	// _imp->libraw->imgdata.params.use_rawspeed = 1;
	_imp->libraw->imgdata.params.no_interpolation= _interpolation_mode == 0;
	_imp->libraw->imgdata.params.highlight = _highlight_mode;

	int err;
	err = _imp->libraw->dcraw_process();
	if(err!= LIBRAW_SUCCESS){
		printf("dcraw process image error\n");
		return NULL;
	}

	image = _imp->libraw->dcraw_make_mem_image(&err);
	
	if (err != LIBRAW_SUCCESS){
		printf("make mem image error\n");
		return NULL;
	}

	_w = image->width;
	_h = image->height;

	// DNGIdt idt = DNGIdt(_imp->libraw->imgdata.rawdata);
	// idt.getDNGIDTMatrix2(_idt.matrix);

	// if(_highlight_mode > 0){
	// 	// Fix WB scale
	// 	float ratio = ( *(std::max_element ( _imp->libraw->imgdata.color.pre_mul, _imp->libraw->imgdata.color.pre_mul+3)) /
	// 					*(std::min_element ( _imp->libraw->imgdata.color.pre_mul, _imp->libraw->imgdata.color.pre_mul+3)) );
	// 	for(int i=0; i < 9; ++i){
	// 		_idt.matrix[i] *= ratio;
	// 	}
	// }

	return (uint16_t*)&image->data;
}

void* Dng_processor::imgdata()
{
	if (_imp->libraw)
		return (void*)&_imp->libraw->imgdata;
	return NULL;
}

