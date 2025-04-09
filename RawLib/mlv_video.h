#pragma once

#include <string>

struct mlv_imp;


class Mlv_video
{
public:
	struct RawInfo {
		bool dualiso_fullres_blending = false;
		bool dualiso_aliasmap = false;
		int dual_iso_mode = false;
		int dualisointerpolation = 0;
		bool fix_focuspixels = true;
		int32_t chroma_smooth = 0;
		float crop_factor = 1.0f;
		float focal_length = 35.0f;
		std::string darkframe_file;
		bool darkframe_enable = false;
		bool darkframe_ok = false;
		std::string darkframe_error;
	};
	mlv_imp* _imp = NULL;
private:
	bool _valid = false;
	bool _locked = false;
	bool _shared = false;

public:


	Mlv_video(std::string filename);
	Mlv_video(const Mlv_video&);
	~Mlv_video();

	void lock(){_locked = true;}
	void unlock(){_locked = false;}
	bool locked(){return _locked;}

	bool valid(){return _valid;}
	void* get_mlv_object();

	void low_level_process(RawInfo& ri);
	uint16_t* get_dng_buffer(uint32_t frame, int& dng_size, bool no_buffer);
	uint32_t get_dng_header_size();
	void free_dng_buffer();
	uint16_t* get_raw_image();
	uint16_t* postprocecessed_raw_buffer();


	uint32_t black_level();
	uint32_t white_level();
	void set_levels(int black, int white);

	float fps();
	uint32_t frame_count();

	uint32_t raw_resolution_x();
	uint32_t raw_resolution_y();

	uint32_t raw_black_level();
	uint32_t raw_white_level();

	std::string camera_name();
	std::string lens_name();
	std::string lens_name_by_id();

	int32_t* get_camera_forward_matrix2();
	void get_camera_forward_matrix2f(float matrix[9]);
	void get_camera_forward_matrix1f(float matrix[9]);
	void get_camera_matrix2f(float matrix[9]);
	void get_camera_matrix1f(float matrix[9]);

	std::string get_camera_make();
	std::string get_camera_model();

	void get_white_balance_coeffs(int temperature, float coeffs[3], float &compensation, bool cam_wb = true);

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

	bool generate_darkframe(const char* path, int in, int out);
	void write_audio(std::string path);
};
