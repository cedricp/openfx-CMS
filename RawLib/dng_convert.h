#pragma once
#include <string>
#include <cstdint>
#include "mlv_video.h"
#include "mathutils.h"

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
	void free_buffer();
	void load_dng(std::string filename);
	uint16_t* get_processed_filebuffer(){return _filebuffer;}
	int bpp(){return _bpp;}
	int black_level(){return _blacklevel;}
	int white_level(){return _whitelevel;}
	Matrix3x3f cam2xyz(){return _cam2xyz;}
	::Vector3f as_shot_neutral(){return _asShotNeutral;}
	Matrix3x3f matrix1(){return _matrix1;}
	Matrix3x3f matrix2(){return _matrix2;}
	float get_baseline_exposure(){return _baseExpo;}
	unsigned short get_calibrate_expo1(){return _calibrate_expo[0];}
	unsigned short get_calibrate_expo2(){return _calibrate_expo[1];}
private:
	struct dngc_impl;
	dngc_impl* _imp;
	unsigned short *_buffer;
	unsigned short *_filebuffer;
	int _w, _h;
	int _interpolation_mode = 3;
	int _highlight_mode = 0;
	int _interp = 0;
	int _bpp = 0;
	int _blacklevel = 0;
	int _whitelevel = 0;
	Matrix3x3f _cam2xyz, _matrix1, _matrix2;
	::Vector3f _asShotNeutral = ::Vector3f(1,1,1);
	float _baseExpo = 1.0;
	unsigned short _calibrate_expo[2] = {1, 1};
};
