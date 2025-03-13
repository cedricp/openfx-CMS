#pragma once
#include "mlv-lib/video_mlv.h"
#include <string>

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
	void* imgdata();
private:
	struct dngc_impl;
	dngc_impl* _imp;
	unsigned short *_buffer;
	int _w, _h;
	int _interpolation_mode = 3;
	int _highlight_mode;
	int _camera_wb;
};
