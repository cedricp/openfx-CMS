#include <cmath>
#include <algorithm>
#include <functional>
#include <Eigen/SVD>
#include <Eigen/LU>

#include <libraw_types.h>
#include "dng_idt.h"
#include "dng_tag_values.h"
#include "mathOps.h"

/*
 * Code borrowed from rawtoaces
 * https://github.com/ampas/rawtoaces
 */

using namespace std;

namespace DNGIdt{


// D65 illum
static const double XYZ_acesrgb_3[3][3] = {
    { 1.0634731317028,      0.00639793641966071,   -0.0157891874506841 },
    { -0.492082784686793,   1.36823709310019,      0.0913444629573544  },
    { -0.0028137154424595,  0.00463991165243123,   0.91649468506889    }
};

static const double XYZ_rec709rgb[3][3] = {
    { 3.2409613, -1.5372608, -0.4986214 },
    { -0.9692159,  1.8758931,  0.0415554  },
    { 0.0556270, -0.2039677,  1.0571218    }
};

static const double AP0toAP1[3][3] = {
    {1.4514393161, -0.2365107469, -0.2149285693},
    {-0.0765537734, 1.1762296998, -0.0996759264},
    {0.0083161484, -0.0060324498, 0.9977163014}
};

DNGIdt::DNGIdt() {
	_cameraToXYZMtx        = vector < double > ( 9, 1.0 );
	_xyz2rgbMatrix1DNG     = vector < double > ( 9, 1.0 );
	_xyz2rgbMatrix2DNG     = vector < double > ( 9, 1.0 );
	_analogBalanceDNG      = vector < double > ( 3, 1.0 );
	_neutralRGBDNG         = vector < double > ( 3, 1.0 );
	_cameraXYZWhitePoint   = vector < double > ( 3, 1.0 );
	_calibrateIllum        = vector < double > ( 2, 1.0 );
	_baseExpo              = 1.0;
}

DNGIdt::DNGIdt ( Mlv_video* mlv, float *wbal ) {
    float matrix1[9], matrix2[9];
    mlv->get_camera_matrix1f(matrix1);
    mlv->get_camera_matrix2f(matrix2);

	_cameraToXYZMtx        = vector < double > ( 9, 1.0 );
	_xyz2rgbMatrix1DNG     = vector < double > ( 9, 1.0 );
	_xyz2rgbMatrix2DNG     = vector < double > ( 9, 1.0 );
	_analogBalanceDNG      = vector < double > ( 3, 1.0 );
	_neutralRGBDNG         = vector < double > ( 3, 1.0 );
	_cameraXYZWhitePoint   = vector < double > ( 3, 1.0 );
	_calibrateIllum        = vector < double > ( 2, 1.0 );

    _baseExpo = 1;//static_cast < double > ( R.color.baseline_exposure );
	_calibrateIllum[0] = static_cast < double > ( lsStandardLightA ); // 2856K - lsStandardLightA  
	_calibrateIllum[1] = static_cast < double > ( lsD65 ); // 6500K - lsD65

    FORI ( 3 ){
         _neutralRGBDNG[i] = static_cast < double > ( 1. / wbal[i] );
    }
    
    FORI ( 9 ) {
		_xyz2rgbMatrix1DNG[i] = static_cast < double > ( matrix1[i] );
		_xyz2rgbMatrix2DNG[i] = static_cast < double > ( matrix2[i] );
	}
}

DNGIdt::~DNGIdt() {
}

double DNGIdt::ccttoMired ( const double cct ) const {
    return 1.0E06 / cct;
}

#define sign(x)		((x) > 0 ? 1 : ( (x) < 0 ? (0-1) : 0))
double DNGIdt::robertsonLength ( const vector < double > & uv,
								 const vector < double > & uvt ) const {

	double t = uvt[2];
	vector < double > slope (2);
	slope[1] = t * slope[0];
	slope[0] = -sign(t) / std::sqrt(1 + t * t);
    
	vector < double > uvr ( uvt.begin(), uvt.begin()+2 );
	return cross2 ( slope, subVectors ( uv, uvr ) ) ;
}

double DNGIdt::lightSourceToColorTemp ( const unsigned short tag ) const {

	if ( tag >= 32768 )
		return ( static_cast < double > ( tag ) ) - 32768.0;

	uint16_t LightSourceEXIFTagValues[][2] = {
		{ 0,    5500 },
		{ 1,    5500 },
		{ 2,    3500 },
		{ 3,    3400 },
		{ 10,   5550 },
		{ 17,   2856 },
		{ 18,   4874 },
		{ 19,   6774 },
		{ 20,   5500 },
		{ 21,   6500 },
		{ 22,   7500 }
	};

	FORI ( countSize ( LightSourceEXIFTagValues ) ) {
		if ( LightSourceEXIFTagValues[i][0] == static_cast < uint16_t > (tag) ) {
			return ( static_cast < double > (LightSourceEXIFTagValues[i][1]) );
		}
	}

	return 5500.0;
}

double DNGIdt::XYZToColorTemperature ( const vector < double > & XYZ ) const {

	vector < double > uv = XYZTouv ( XYZ );
	int Nrobert = countSize ( Robertson_uvtTable );
	int i;

	double mired;
	double RDthis = 0.0, RDprevious = 0.0;

	for ( i = 0; i < Nrobert; i++ ) {
		vector < double > robertson ( Robertson_uvtTable[i],
									  Robertson_uvtTable[i] + countSize(Robertson_uvtTable[i]) );
		if (( RDthis = robertsonLength ( uv, robertson ) ) <= 0.0 )
			break;
		RDprevious = RDthis;
	}
	if ( i <= 0 )
		mired = RobertsonMired[0];
	else if ( i >= Nrobert )
		mired = RobertsonMired[Nrobert - 1];
	else
		mired = RobertsonMired[i-1] + RDprevious * ( \
				RobertsonMired[i] - RobertsonMired[i-1] )\
				/( RDprevious-RDthis );

	double cct = 1.0e06 / mired;
	cct = std::max ( 2000.0, std::min ( 50000.0, cct ) );

	return cct;
}

vector < double > DNGIdt::XYZtoCameraWeightedMatrix ( const double & mir0,
													  const double & mir1,
													  const double & mir2 ) const {

	double weight = std::max ( 0.0, std::min ( 1.0, (mir1 - mir0) / (mir1 - mir2) ) );
	vector < double > result = subVectors ( _xyz2rgbMatrix2DNG, _xyz2rgbMatrix1DNG );
	scaleVector ( result, weight );
	result = addVectors ( result, _xyz2rgbMatrix1DNG );

	return result;
}

vector < double > DNGIdt::findXYZtoCameraMtx ( const vector < double > & neutralRGB ) const {

	if ( _calibrateIllum.size() == 0 ) {
		fprintf ( stderr, " No calibration illuminants were found. \n " );
		return _xyz2rgbMatrix1DNG;
	}

	if ( neutralRGB.size() == 0 ) {
		fprintf ( stderr, " no neutral RGB values were found. \n " );
		return _xyz2rgbMatrix1DNG;
	}

	double cct1 = lightSourceToColorTemp ( static_cast < const unsigned short > ( _calibrateIllum[0] ) );
	double cct2 = lightSourceToColorTemp ( static_cast < const unsigned short > ( _calibrateIllum[1] ) );

	double mir1 = ccttoMired ( cct1 );
	double mir2 = ccttoMired ( cct2 );

	double maxMir = ccttoMired ( 2000.0 );
	double minMir = ccttoMired ( 50000.0 );

	double lomir = std::max ( minMir, std::min ( maxMir, std::min ( mir1, mir2 ) ) );
	double himir = std::max ( minMir, std::min ( maxMir, std::max ( mir1, mir2 ) ) );
	double mirStep = std::max ( 5.0, ( himir - lomir ) / 50.0 );

	double mir = 0.0, lastMired = 0.0, estimatedMired = 0.0, lerror = 0.0, lastError = 0.0, smallestError = 0.0;

	for ( mir = lomir; mir < himir;  mir += mirStep ) {
		lerror = mir - ccttoMired ( XYZToColorTemperature ( mulVector \
								   ( invertV ( XYZtoCameraWeightedMatrix ( mir, mir1, mir2 ) ),\
									_neutralRGBDNG ) ) );

		if ( std::fabs( lerror - 0.0 ) <= 1e-09 ) {
			estimatedMired = mir;
			break;
		}
		if ( std::fabs( mir - lomir - 0.0 ) > 1e-09
			 && lerror * lastError <= 0.0 ) {
			estimatedMired = mir + ( lerror / ( lerror-lastError ) * ( mir - lastMired ) );
			break;
		}
		if ( std::fabs( mir - lomir ) <= 1e-09
			 || std::fabs ( lerror ) < std::fabs ( smallestError ) )    {
			estimatedMired = mir ;
			smallestError = lerror;
		}

		lastError = lerror;
		lastMired = mir;
	}

	return XYZtoCameraWeightedMatrix ( estimatedMired, mir1, mir2 );
}

vector < double > DNGIdt::colorTemperatureToXYZ ( const double & cct ) const {

	double mired = 1.0e06 / cct;
	vector < double > uv ( 2, 1.0 );

	int Nrobert = countSize (Robertson_uvtTable);
	int i;

	for ( i = 0; i < Nrobert; i++ ) {
		if ( RobertsonMired[i] >= mired )
			break;
	}

	if ( i <= 0 ) {
		uv = vector < double > ( Robertson_uvtTable[0], Robertson_uvtTable[0] + 2 );
	}
	else if ( i >= Nrobert ) {
		uv = vector < double > ( Robertson_uvtTable[Nrobert - 1], Robertson_uvtTable[Nrobert - 1] + 2 );
	}
	else {
		double weight = ( mired - RobertsonMired[i-1] ) / ( RobertsonMired[i] - RobertsonMired[i-1] );

		vector < double > uv1 ( Robertson_uvtTable[i], Robertson_uvtTable[i] + 2 );
		scaleVector ( uv1, weight );

		vector < double > uv2 ( Robertson_uvtTable[i-1], Robertson_uvtTable[i-1] + 2 );
		scaleVector ( uv2, 1.0 - weight );

		uv = addVectors ( uv1, uv2 );
	}

	return uvToXYZ(uv);
}

vector < double > DNGIdt::matrixRGBtoXYZ ( const double chromaticities[][2] ) const {
	vector < double > rXYZ = xyToXYZ ( vector< double > ( chromaticities[0], chromaticities[0] + 2 ) );
	vector < double > gXYZ = xyToXYZ ( vector< double > ( chromaticities[1], chromaticities[1] + 2 ) );
	vector < double > bXYZ = xyToXYZ ( vector< double > ( chromaticities[2], chromaticities[2] + 2 ) );
	vector < double > wXYZ = xyToXYZ ( vector< double > ( chromaticities[3], chromaticities[3] + 2 ) );

	vector < double > rgbMtx(9);
	FORI(3) {
		rgbMtx[0+i*3] = rXYZ[i];
		rgbMtx[1+i*3] = gXYZ[i];
		rgbMtx[2+i*3] = bXYZ[i];
	}

	scaleVector ( wXYZ, 1.0 / wXYZ[1] );

	vector < double > channelgains = mulVector ( invertV ( rgbMtx ), wXYZ, 3 );
	vector < double > colorMatrix  = mulVector ( rgbMtx, diagV ( channelgains ), 3 );

	return colorMatrix;
}

void DNGIdt::getCameraXYZMtxAndWhitePoint ( ) {
	_cameraToXYZMtx = invertV ( findXYZtoCameraMtx ( _neutralRGBDNG ) );
	assert ( std::fabs ( sumVector ( _cameraToXYZMtx ) - 0.0 ) > 1e-09 );

	scaleVector ( _cameraToXYZMtx, std::pow ( 2.0, _baseExpo ) );

	if ( _neutralRGBDNG.size() > 0 ) {
		_cameraXYZWhitePoint = mulVector ( _cameraToXYZMtx, _neutralRGBDNG );
	} else {
		_cameraXYZWhitePoint = colorTemperatureToXYZ ( lightSourceToColorTemp ( _calibrateIllum[0] ) );
	}

	scaleVector ( _cameraXYZWhitePoint, 1.0 / _cameraXYZWhitePoint[1] );
	assert ( sumVector ( _cameraXYZWhitePoint ) != 0);

	return;
}

vector < vector < double > > DNGIdt::getDNGCATMatrix ( bool rec709 ) {
	vector < double > deviceWhiteV ( 3, 1.0 );
	getCameraXYZMtxAndWhitePoint ( );
	vector < double > outputRGBtoXYZMtx = matrixRGBtoXYZ ( rec709 ? chromaticitiesREC709 : chromaticitiesACES );
	vector < double > outputXYZWhitePoint = mulVector ( outputRGBtoXYZMtx, deviceWhiteV );
	vector < vector < double > > chadMtx = getCAT ( _cameraXYZWhitePoint, outputXYZWhitePoint );

	return chadMtx;
}

// Colorspace
// 0 : AP0
// 1 : AP1
// 2 : rec.709
void DNGIdt::getDNGIDTMatrix (float* mat, int colorspace)
{
	vector < vector < double > > chadMtx = getDNGCATMatrix ( colorspace == 2 );
	vector < double > XYZ_acesrgb (9), CAT (9);

	FORIJ ( 3, 3 ) {
		XYZ_acesrgb[i*3+j] = colorspace == 2 ? XYZ_rec709rgb[i][j] : XYZ_acesrgb_3[i][j];
		CAT[i*3+j] = chadMtx[i][j];
	}

	vector < double > matrixAP0 = mulVector ( XYZ_acesrgb, CAT, 3 );
    vector < vector < double > > DNGIDTMatrix ( 3, vector < double > (3) );
    if(colorspace == 1){
        vector < double > ap0toap1(9);
        FORIJ ( 3, 3 ) ap0toap1[i*3+j] = AP0toAP1[i][j];

        vector< double > matrixAP1 = mulVector ( ap0toap1, matrixAP0, 3 );
        // assert ( std::fabs( sumVectorM ( DNGIDTMatrix ) - 0.0 ) > 1e-09 );
        // FORIJ ( 3, 3 ) DNGIDTMatrix[i][j] = matrixAP1[i*3+j];
        FORI(9) mat[i] = matrixAP1[i];
    } else {
        // FORIJ ( 3, 3 ) DNGIDTMatrix[i][j] = matrixAP0[i*3+j];
        // assert ( std::fabs( sumVectorM ( DNGIDTMatrix ) - 0.0 ) > 1e-09 );
        FORI(9) mat[i] = matrixAP0[i];
    }


}
}