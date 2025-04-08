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
	uint16_t* get_processed_image(uint8_t* buffer, size_t buffersize, bool compute_aces_matrix);
	int width(){return _w;}
	int height(){return _h;}
	void set_interpolation(int i){_interpolation_mode = i;}
	void set_highlight(int i){_highlight_mode = i;}
	void set_camera_wb(bool wb){_camera_wb = wb;}
	void free_buffer();
	void setColorspace(int cs){_colorspace = cs;}
	float* get_idt_matrix(){return _idt_matrix;}
	float get_wbratio(){return _wb_compensation;}
	void set_color_temperature(int color_temp){_color_temperature = color_temp;}
private:
	struct dngc_impl;
	dngc_impl* _imp;
	unsigned short *_buffer;
	int _w, _h;
	int _interpolation_mode = 3;
	int _highlight_mode = 0;
	int _camera_wb;
	int _interp = 0;
	float _wb_compensation = 1;
	float _idt_matrix[9];
	int _colorspace;
	int _color_temperature;
	Mlv_video* _mlv_video;
};
