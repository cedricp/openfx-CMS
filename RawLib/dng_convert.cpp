#include "dng_convert.h"
#include <libraw.h>
#include <sys/stat.h>
#include <algorithm>
#include "idt/dng_idt.h"
extern "C"{
#include <dng/dng.h>
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

uint16_t* Dng_processor::get_processed_image(uint8_t* buffer, size_t buffersize, bool compute_aces_matrix)
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
	11 - DHT intepolation
	12 - Modified AHD intepolation (by Anton Petrusevich)
	*/

	// output_color -> linear, sRGB, Adobe, Wide, ProPhoto, XYZ, ACES, DCI-P3, Rec. 2020
	_imp->libraw->imgdata.params.output_color = 5; // XYZ is the one we want -> every colorspaces can be converted from it
	_imp->libraw->imgdata.params.output_bps = 16;
	_imp->libraw->imgdata.params.gamm[0] = 1.0;
	_imp->libraw->imgdata.params.gamm[1] = 1.0;
	// See debayer method above
	_imp->libraw->imgdata.params.user_qual = _interpolation_mode < 5 ? _interpolation_mode : _interpolation_mode + 6;
	_imp->libraw->imgdata.params.use_camera_matrix = 1;
	_imp->libraw->imgdata.params.use_auto_wb = 0;
	// threshold-> Parameter for noise reduction through wavelet denoising.
	_imp->libraw->imgdata.params.threshold = 0.; 
	_imp->libraw->imgdata.params.bright = 1.;
	_imp->libraw->imgdata.params.no_auto_bright = 1.;
	_imp->libraw->imgdata.params.half_size = 0;
	_imp->libraw->imgdata.params.use_camera_wb = _camera_wb;
	_ratio = 1;
	if (!_camera_wb && compute_aces_matrix){
		int32_t wbal[6];
		::get_white_balance(_wb_coeffs, wbal, _camid);
		float wbrgb[3] = {float(wbal[1]) / 1000000., float(wbal[3]) / 1000000., float(wbal[5]) / 1000000.};
		if(_highlight_mode > 0){
			_ratio = *std::max_element(wbrgb, wbrgb+3) / *std::min_element(wbrgb, wbrgb+3);
		}
		// Set white balance coefficients
		_imp->libraw->imgdata.params.user_mul[0] = wbrgb[0];
		_imp->libraw->imgdata.params.user_mul[1] = wbrgb[1];
		_imp->libraw->imgdata.params.user_mul[2] = wbrgb[2];
		_imp->libraw->imgdata.params.user_mul[3] = wbrgb[1];
	}
	// _imp->libraw->imgdata.params.use_rawspeed = 1;
	_imp->libraw->imgdata.params.no_interpolation= 0;
	// Highlight mode is highlight reconstruction
	_imp->libraw->imgdata.params.highlight = _highlight_mode;

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

	if (!compute_aces_matrix){
		DNGIdt::DNGIdt idt(&_imp->libraw->imgdata.rawdata);
		idt.getDNGIDTMatrix2(_idt_matrix, _ap1_matrix);

		if(_highlight_mode > 0){
			// Fix WB scale
			float ratio = ( *(std::max_element ( _imp->libraw->imgdata.color.pre_mul, _imp->libraw->imgdata.color.pre_mul+3)) /
							*(std::min_element ( _imp->libraw->imgdata.color.pre_mul, _imp->libraw->imgdata.color.pre_mul+3)) );
			for(int i=0; i < 9; ++i){
				_idt_matrix[i] *= ratio;
			}
		}
	}

	return (uint16_t*)&_imp->_image->data;
}

void Dng_processor::free_buffer()
{
	if (_imp->libraw){
		free(_imp->_image);
		_imp->_image = 0;
	}
}

