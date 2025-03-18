#pragma once

#include <string>
#include <mlv.h>

struct mlv_imp;


class Mlv_video
{
public:
	struct RawInfo {
		bool fix_focuspixels = true;
		int32_t chroma_smooth = 0;
		int32_t temperature = -1;
		int interpolation = 4;
		int highlight = 3;
		float crop_factor = 1.0f;
		float focal_length = 35.0f;
		std::string darkframe_file;
		bool darkframe_enable = false;
		bool darkframe_ok = false;
		std::string darkframe_error;
	};
	mlv_imp* _imp = NULL;
private:
	RawInfo _rawinfo;
	bool _valid = false;

public:


	Mlv_video(std::string filename);
	~Mlv_video();

	bool valid(){return _valid;}
	void* get_mlv_object();

	mlv_wbal_hdr_t get_wb_object();

	uint16_t* get_dng_buffer(uint32_t frame, const RawInfo& ri, int& dng_size);
	uint32_t get_dng_header_size();
	uint16_t* get_raw_image();
	uint16_t* unpacked_buffer(uint16_t* input_buffer);

	uint32_t black_level();
	uint32_t white_level();

	float fps();
	uint32_t frame_count();

	uint32_t raw_resolution_x();
	uint32_t raw_resolution_y();
	std::string camera_name();
	std::string lens_name();
	std::string lens_name_by_id();
	int get_camid();
	float focal_length();
	float focal_dist();
	float aperture();
	float crop_factor();
	float final_crop_factor();
	uint32_t iso();
	uint32_t shutter_speed();
	int pixel_binning_x();
    int pixel_binning_y();
	int sampling_factor_x();
    int sampling_factor_y();
	int bpp();
	void sensor_resolulion(int& x, int& y);

	bool generate_darkframe(int in, int out);
};
