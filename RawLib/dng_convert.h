#pragma once
#include <string>
#include <cstdint>
#include "mlv_video.h"

class Dng_processor
{
public:
	Dng_processor(Mlv_video* mlv_video);
	~Dng_processor();

	void unpack(uint8_t* buffer, size_t buffersize);
	uint16_t* get_processed_image(uint8_t* buffer, size_t buffersize, float wbrgb[4]);
	int width(){return _w;}
	int height(){return _h;}
	void set_interpolation(int i){_interpolation_mode = i;}
	void set_highlight(int i){_highlight_mode = i;}
	void free_buffer();
private:
	struct dngc_impl;
	dngc_impl* _imp;
	unsigned short *_buffer;
	int _w, _h;
	int _interpolation_mode = 3;
	int _highlight_mode = 0;
	int _interp = 0;
	Mlv_video* _mlv_video;
};
