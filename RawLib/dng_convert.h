#pragma once
#include <string>
#include <cstdint>
#include "mlv_video.h"

class Dng_processor
{
public:
	Dng_processor();
	~Dng_processor();

	void unpack(uint8_t* buffer, size_t buffersize);
	uint16_t* get_processed_image(uint8_t* buffer, size_t buffersize);
	int width(){return _w;}
	int height(){return _h;}
	void set_interpolation(int i){_interpolation_mode = i;}
	void set_highlight(int i){_highlight_mode = i;}
	void set_camera_wb(bool wb){_camera_wb = wb;}
	void set_colorspace(int c){_colorspace = c;}
	void free_buffer();
	void set_wb_coeffs(mlv_wbal_hdr_t wb_coeffs){
		_wb_coeffs = wb_coeffs;
	}
	void set_camid(int cid){_camid = cid;}
private:
	struct dngc_impl;
	dngc_impl* _imp;
	unsigned short *_buffer;
	int _w, _h;
	int _interpolation_mode = 3;
	int _highlight_mode = 0;
	int _camera_wb;
	int _colorspace = 0;
	int _interp = 0;
	int _camid;
	mlv_wbal_hdr_t _wb_coeffs;
};
