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
	uint16_t* get_processed_image(uint8_t* buffer, size_t buffersize, bool apply_wb_coeffs);
	int width(){return _w;}
	int height(){return _h;}
	void set_interpolation(int i){_interpolation_mode = i;}
	void set_highlight(int i){_highlight_mode = i;}
	void set_camera_wb(bool wb){_camera_wb = wb;}
	void free_buffer();
	void set_wb_coeffs(mlv_wbal_hdr_t wb_coeffs){_wb_coeffs = wb_coeffs;}
	void set_camid(int cid){_camid = cid;}
	float* get_idt_matrix(){return _idt_matrix;}
	float get_wbratio(){return _ratio;}
private:
	struct dngc_impl;
	dngc_impl* _imp;
	unsigned short *_buffer;
	int _w, _h;
	int _interpolation_mode = 3;
	int _highlight_mode = 0;
	int _camera_wb;
	int _interp = 0;
	int _camid;
	float _ratio = 1;
	float _idt_matrix[9];
	mlv_wbal_hdr_t _wb_coeffs;
};
