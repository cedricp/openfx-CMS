#pragma once
#include <string>
#include <cstdint>
#include "mlv_video.h"
#include "mathutils.h"
#include <mlv.h>

using namespace MathUtils;

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
	bool load_dng(std::string filename);
	uint16_t* get_processed_filebuffer();
	int bpp(){return _bpp;}
	int black_level(){return _blacklevel;}
	int white_level(){return _whitelevel;}
	Vector3f as_shot_neutral(){return _asShotNeutral;}
	Matrix3x3f matrix1(){return _matrix1;}
	Matrix3x3f matrix2(){return _matrix2;}
	float get_baseline_exposure(){return _baseExpo;}
	unsigned short get_calibrate_expo1(){return _calibrate_expo[0];}
	unsigned short get_calibrate_expo2(){return _calibrate_expo[1];}
	void get_white_balance_coeffs(int temperature, float coeffs[3], bool cam_wb);
	void get_white_balance(mlv_wbal_hdr_t wbal_hdr, int32_t *wbal);
	float* cam_mult(){return _cam_mult;}
	std::string camera_make(){return _camera_make;}
	std::string camera_model(){return _camera_model;}

	void lock(){dng_lock = true;}
	void unlock(){dng_lock = false;}
	bool locked(){return dng_lock;}
	
	private:
	void kelvin_green_to_multipliers(double temperature, double green, double chanMulArray[3]);
	struct dngc_impl;
	dngc_impl* _imp;
	unsigned short *_buffer;
	int _w, _h;
	int _interpolation_mode = 3;
	int _highlight_mode = 0;
	int _interp = 0;
	int _bpp = 0;
	int _blacklevel = 0;
	int _whitelevel = 0;
	int _camera_wb = 5500;
	Matrix3x3f _matrix1, _matrix2;
	Vector3f _asShotNeutral = ::Vector3f(1,1,1);
	float _baseExpo = 1.0;
	unsigned short _calibrate_expo[2] = {17, 21};
	bool dng_lock = false;
	float _cam_mult[3] = {1.0, 1.0, 1.0};

	std::string _camera_model;
	std::string _camera_make;
};
