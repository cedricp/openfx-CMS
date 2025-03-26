#pragma once


#include <vector>
#include "../mlv_video.h"

namespace DNGIdt{
	class DNGIdt {
		public:
			DNGIdt();
			#ifndef IDT_NOGNG
			DNGIdt (libraw_rawdata_t* rawdata_type);
			#endif
			DNGIdt (Mlv_video* mlv, float *wbal );
			virtual ~DNGIdt();

			double ccttoMired ( const double cct ) const;
			double robertsonLength ( const std::vector < double > & uv,
									const std::vector < double > & uvt ) const;
			double lightSourceToColorTemp ( const unsigned short tag ) const;
			double XYZToColorTemperature ( const std::vector < double > & XYZ ) const;

			std::vector < double > XYZtoCameraWeightedMatrix ( const double & mir,
														const double & mir1,
														const double & mir2 ) const;

			std::vector < double > findXYZtoCameraMtx ( const std::vector < double > & neutralRGB ) const;
			std::vector < double > colorTemperatureToXYZ ( const double & cct ) const;
			std::vector < double > matrixRGBtoXYZ ( const double chromaticities[][2] ) const;

			std::vector < std::vector < double > > getDNGCATMatrix ( );
			std::vector < std::vector < double > > getDNGIDTMatrix ( );
			void getDNGIDTMatrix2 ( float* out_matrix, bool AP1 );
			void getCameraXYZMtxAndWhitePoint ( );

		private:
			std::vector < double >  _cameraToXYZMtx;
			std::vector < double >  _xyz2rgbMatrix1DNG;
			std::vector < double >  _xyz2rgbMatrix2DNG;
			std::vector < double >  _analogBalanceDNG;
			std::vector < double >  _neutralRGBDNG;
			std::vector < double >  _cameraXYZWhitePoint;
			std::vector < double >  _calibrateIllum;
			double _baseExpo;
	};
}
