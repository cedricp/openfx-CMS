#include "mathOps.h"
#include "spectral_idt.h"
#include "color_data.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <cassert>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cmath>

using namespace boost::property_tree;

namespace SSIDT {
Illum::Illum()
{
    _inc = 5;
}

Illum::Illum( string type )
{
    _type = type;
    _inc  = 5;
}

Illum::~Illum()
{
    vector<double>().swap( _data );
}

//	=====================================================================
//	Set the type of Illuminant
//
//	inputs:
//      const char *: type (from user input)
//
//	outputs:
//		void: _type will be assigned a value to (private member)

void Illum::setIllumType( const string &type )
{
    assert( !type.empty() );
    _type = type;

    return;
}

//	=====================================================================
//	Set the increment of Illuminant SPD
//
//	inputs:
//      int : inc
//
//	outputs:
//		void: _inc  will be assigned with a value (private member)

void Illum::setIllumInc( const int &inc )
{
    _inc = inc;

    return;
}

//	=====================================================================
//	Set the index of Illuminant SPD
//
//	inputs:
//      int : index
//
//	outputs:
//		void: _index  will be assigned with a value (private member)

void Illum::setIllumIndex( const double &index )
{
    _index = index;

    return;
}

void Illum::loadDefaultSPD(){
    _data.clear();
    
    for (int i = 380, j = 0; i <= 780; i+=5, j++)
    {
        _data.push_back(iso7589_stutung_380_780[j]);
        if (i == 550){
            _index = iso7589_stutung_380_780[j];
        }
    }
}

//	=====================================================================
//	Read the Illuminant data from JSON file(s)
//
//	inputs:
//		string: path to the Illuminant data file
//      const char *: type of light source if user specifies
//
//	outputs:
//		int: If successufully parsed, private data members (e.g., _data)
//           will be filled and return 1; Otherwise, return 0

int Illum::readSPD( const string &path, const string &type )
{
    try
    {
        // using libraries from boost::property_tree
        ptree pt;
        read_json( path, pt );

        const string stype = pt.get<string>( "header.illuminant" );
        if ( type.compare( stype ) != 0 && type.compare( "na" ) != 0 )
            return 0;

        _type = stype;

        vector<int> wavs;
        int         dis;

        BOOST_FOREACH (
            ptree::value_type &row, pt.get_child( "spectral_data.data.main" ) )
        {
            wavs.push_back( atoi( ( row.first ).c_str() ) );

            if ( wavs.size() == 2 )
                dis = wavs[1] - wavs[0];
            else if (
                wavs.size() > 2 &&
                wavs[wavs.size() - 1] - wavs[wavs.size() - 2] != dis )
            {
                fprintf(
                    stderr,
                    "Please double check the Light "
                    "Source data (e.g. the increment "
                    "should be uniform from 380nm to 780nm).\n" );
                exit( -1 );
            }

            if ( wavs[wavs.size() - 1] < 380 || wavs[wavs.size() - 1] % 5 )
                continue;
            else if ( wavs[wavs.size() - 1] > 780 )
                break;

            BOOST_FOREACH ( ptree::value_type &cell, row.second )
            {
                _data.push_back( cell.second.get_value<double>() );
                if ( wavs[wavs.size() - 1] == 550 )
                    _index = cell.second.get_value<double>();
            }
        }

        _inc = dis;
    }
    catch ( std::exception const &e )
    {
        std::cerr << e.what() << std::endl;
    }

    if ( _data.size() != 81 )
    {
        fprintf(
            stderr,
            "Please double check the Light "
            "Source data (e.g. the increment "
            "should be 5nm from 380nm to 780nm).\n" );
        exit( 1 );
    }

    return 1;
}

//	=====================================================================
//	Calculate the chromaticity values based on cct
//
//	inputs:
//      const int: cct / correlated color temperature
//
//	outputs:
//		vector <double>: xy / chromaticity values
//

vector<double> Illum::cctToxy( const double &cctd ) const
{
    //        assert( cctd >= 4000 && cct <= 25000 );

    vector<double> xy( 2, 1.0 );
    if ( cctd >= 4002.15 && cctd <= 7003.77 )
        xy[0] =
            ( 0.244063 + 99.11 / cctd +
              2.9678 * 1000000 / ( std::pow( cctd, 2 ) ) -
              4.6070 * 1000000000 / ( std::pow( cctd, 3 ) ) );
    else
        xy[0] =
            ( 0.237040 + 247.48 / cctd +
              1.9018 * 1000000 / ( std::pow( cctd, 2 ) ) -
              2.0064 * 1000000000 / ( std::pow( cctd, 3 ) ) );

    xy[1] = -3.0 * ( std::pow( xy[0], 2 ) ) + 2.87 * xy[0] - 0.275;

    return xy;
}

//	=====================================================================
//	Calculate spectral power distribution(SPD) of CIE standard daylight
//  illuminant based on the requested Correlated Color Temperature
//	input value(s):
//
//      const int: cct / correlated color temperature
//
//	outputs:
//		int: If successufully processed, private data members (e.g., _data)
//           will be filled and return 1; Otherwise, return 0

void Illum::calDayLightSPD( const int &cct )
{
    assert( ( s_series[53].wl - s_series[0].wl ) % _inc == 0 );

    double cctd = 1.0;
    if ( cct >= 40 && cct <= 250 )
        cctd = cct * 100 * 1.4387752 / 1.438;
    else if ( cct >= 4000 && cct <= 25000 )
        cctd = cct * 1.0;
    else
    {
        fprintf(
            stderr,
            "The range of Correlated Color Temperature for "
            "Day Light should be from 4000 to 25000. \n" );
        exit( 1 );
    }

    if ( _data.size() > 0 )
        _data.clear();
    if ( !_type.size() )
    {
        char buffer[10];
        snprintf( buffer, 10, "%d", cct );
        _type = "d" + string( buffer );
    }

    vector<int>    wls0, wls1;
    vector<double> s00, s10, s20, s01, s11, s21;
    vector<double> xy = cctToxy( cctd );

    double m0 = 0.0241 + 0.2562 * xy[0] - 0.7341 * xy[1];
    double m1 = ( -1.3515 - 1.7703 * xy[0] + 5.9114 * xy[1] ) / m0;
    double m2 = ( 0.03000 - 31.4424 * xy[0] + 30.0717 * xy[1] ) / m0;

    FORI( 54 )
    {
        wls0.push_back( s_series[i].wl );
        s00.push_back( s_series[i].RGB[0] );
        s10.push_back( s_series[i].RGB[1] );
        s20.push_back( s_series[i].RGB[2] );
    }

    int size = ( s_series[53].wl - s_series[0].wl ) / _inc + 1;
    FORI( size )
    wls1.push_back( s_series[0].wl + _inc * i );

    s01 = interp1DLinear( wls0, wls1, s00 );
    clearVM( s00 );
    s11 = interp1DLinear( wls0, wls1, s10 );
    clearVM( s10 );
    s21 = interp1DLinear( wls0, wls1, s20 );
    clearVM( s20 );

    clearVM( wls0 );
    clearVM( wls1 );

    FORI( size )
    {
        int index = s_series[0].wl + _inc * i;
        if ( index >= 380 && index <= 780 )
        {
            _data.push_back( s01[i] + m1 * s11[i] + m2 * s21[i] );
            if ( index == 550 )
                _index = _data[_data.size() - 1];
        }
    }

    clearVM( s01 );
    clearVM( s11 );
    clearVM( s21 );
}

//	=====================================================================
//	Fetch Illuminant SPD data
//
//	inputs:
//      N/A
//
//	outputs:
//		const vector < double > : the SPD data of the Illuminant

const vector<double> Illum::getIllumData() const
{
    return _data;
}

//	=====================================================================
//	Fetch the type of the Illuminant
//
//	inputs:
//      N/A
//
//	outputs:
//		const string _type : the type of Illuminant

string Illum::getIllumType() const
{
    return _type;
}

//	=====================================================================
//	Fetch the interval/increment of Illuminant SPD data
//
//	inputs:
//      N/A
//
//	outputs:
//		const int : the interval/increment of the Illuminant SPD data

int Illum::getIllumInc() const
{
    return _inc;
}

//   =====================================================================
//  Fetch the index value of Illuminant SPD data at 550nm
//
//  inputs:
//      N/A
//
//  outputs:
//      const int : the index value of the Illuminant SPD data at 550nm

double Illum::getIllumIndex() const
{
    return _index;
}

//	=====================================================================
//    Generates blackbody curve(s) of a given temperature
//
//      const int: temp / temperature
//
//	outputs:
//		int: If successufully processed, private data members (e.g., _data)
//           will be filled and return 1; Otherwise, return 0

void Illum::calBlackBodySPD( const int &cct )
{
    if ( cct < 1500 || cct >= 4000 )
    {
        fprintf(
            stderr,
            "The range of Color Temperature for BlackBody "
            "should be from 1500 to 3999. \n" );
        exit( 1 );
    }

    if ( _data.size() > 0 )
        _data.clear();
    if ( !_type.size() )
    {
        char buffer[10];
        snprintf( buffer, 10, "%d", cct );
        _type = string( buffer ) + "k";
    }

    for ( int wav = 380; wav <= 780; wav += 5 )
    {
        double lambda = wav / 1e9;
        double c1     = 2 * bh * ( std::pow( bc, 2 ) );
        double c2     = ( bh * bc ) / ( bk * lambda * cct );
        _data.push_back(
            c1 * M_PI / ( std::pow( lambda, 5 ) * ( std::exp( c2 ) - 1 ) ) );
    }
}

// ------------------------------------------------------//

Spst::Spst()
{
    _brand      = nullptr;
    _model      = nullptr;
    _increment  = 5;
    _spstMaxCol = -1;

    for ( int i = 0; i < 81; i++ )
    {
        _rgbsen.push_back( RGBSen() );
    }
}

Spst::Spst( const Spst &spstobject )
{
    assert( spstobject._brand != nullptr && spstobject._model != nullptr );

    size_t lenb = strlen( spstobject._brand );
    assert( lenb < 64 );

    if ( lenb > 64 )
        lenb = 64;

    _brand = (char *)malloc( lenb + 1 );
    memset( _brand, 0x0, lenb );
    memcpy( _brand, spstobject._brand, lenb );
    _brand[lenb] = '\0';

    size_t lenm = strlen( spstobject._model );
    assert( lenm < 64 );

    if ( lenm > 64 )
        lenm = 64;

    _model = (char *)malloc( lenm + 1 );
    memset( _model, 0x0, lenm );
    memcpy( _model, spstobject._model, lenm );
    _model[lenm] = '\0';

    _increment  = spstobject._increment;
    _spstMaxCol = spstobject._spstMaxCol;
    _rgbsen     = spstobject._rgbsen;
}

Spst::~Spst()
{
    delete _brand;
    delete _model;

    vector<RGBSen>().swap( _rgbsen );
}

//	=====================================================================
//	Fetch the brand of camera
//
//	inputs:
//      N/A
//
//	outputs:
//		const char *: the name of camera brand

const char *Spst::getBrand() const
{
    return _brand;
}

//	=====================================================================
//	Fetch the model of the camera
//
//	inputs:
//      N/A
//
//	outputs:
//		const char *: the model of the camera

const char *Spst::getModel() const
{
    return _model;
}

//	=====================================================================
//	Fetch wavelength increment value of the camera sensitivity
//
//	inputs:
//      N/A
//
//	outputs:
//		const uint8_t: Wavelength increment value (e.g., 5nm, 10nm) of
//                     the camera sensitivity

uint8_t Spst::getWLIncrement() const
{
    return _increment;
}

//	=====================================================================
//	Fetch the sensitivity data of the camera (reading from the file)
//
//	inputs:
//      N/A
//
//	outputs:
//		const vector <RGBSen>: the sensitivity (in vector) of the camera

vector<RGBSen> Spst::getSensitivity() const
{
    return _rgbsen;
}

//	=====================================================================
//	Fetch the brand of camera
//
//	inputs:
//      N/A
//
//	outputs:
//		char *: the name of camera brand

char *Spst::getBrand()
{
    return _brand;
}

//	=====================================================================
//	Fetch the model of the camera
//
//	inputs:
//      N/A
//
//	outputs:
//	    char *: the model of the camera

char *Spst::getModel()
{
    return _model;
}

//	=====================================================================
//	Fetch the wavelength increment value of the camera sensitivity
//
//	inputs:
//      N/A
//
//	outputs:
//	    uint8_t: Wavelength increment value (e.g., 5nm, 10nm) of the
//               camera's sensitivity

int Spst::getWLIncrement()
{
    return _increment;
}

//	=====================================================================
//	Fetch the sensitivity data of the camera (reading from the file)
//
//	inputs:
//		string: path to the camera sensitivity file
//      const char *: camera maker  (from libraw)
//      const char *: camera model  (from libraw)
//
//	outputs:
//		int : the private data members (e.g., _rgbsen) will be filled

int Spst::loadSpst( const string &path, const char *maker, const char *model )
{
    assert( path.length() > 0 && maker != nullptr && model != nullptr );

    vector<RGBSen> rgbsen;
    vector<double> max( 3, dmin );

    try
    {
        ptree pt;
        read_json( path, pt );

        string cmaker = pt.get<string>( "header.manufacturer" );
        if ( cmp_str( maker, cmaker.c_str() ) )
            return 0;
        setBrand( cmaker.c_str() );

        string cmodel = pt.get<string>( "header.model" );
        if ( cmp_str( model, cmodel.c_str() ) )
            return 0;
        setModel( cmodel.c_str() );

        vector<int> wavs;
        int         inc;

        BOOST_FOREACH (
            ptree::value_type &row, pt.get_child( "spectral_data.data.main" ) )
        {
            wavs.push_back( atoi( ( row.first ).c_str() ) );

            if ( wavs.size() == 2 )
                inc = wavs[1] - wavs[0];
            else if (
                wavs.size() > 2 &&
                wavs[wavs.size() - 1] - wavs[wavs.size() - 2] != inc )
            {
                fprintf(
                    stderr,
                    "Please double check the Camera "
                    "Sensitivity data (e.g. the increment "
                    "should be uniform from 380nm to 780nm).\n" );
                exit( 1 );
            }

            if ( wavs[wavs.size() - 1] < 380 || wavs[wavs.size() - 1] % 5 )
                continue;
            else if ( wavs[wavs.size() - 1] > 780 )
                break;

            vector<double> data;
            BOOST_FOREACH ( ptree::value_type &cell, row.second )
                data.push_back( cell.second.get_value<double>() );

            // ensure there are three components
            assert( data.size() == 3 );

            RGBSen tmp_sen( data[0], data[1], data[2] );

            if ( tmp_sen._RSen > max[0] )
                max[0] = tmp_sen._RSen;
            if ( tmp_sen._GSen > max[1] )
                max[1] = tmp_sen._GSen;
            if ( tmp_sen._BSen > max[2] )
                max[2] = tmp_sen._BSen;

            //                printf( "\"%i\": [ %18.13f,  %18.13f,  %18.13f ], \n",
            //                        wavs[wavs.size()-1],
            //                        data[0],
            //                        data[1],
            //                        data[2] );
            //
            rgbsen.push_back( tmp_sen );
        }
        setWLIncrement( inc );
    }
    catch ( std::exception const &e )
    {
        std::cerr << e.what() << std::endl;
    }

    // it can be updated if there is a broader spectrum
    // (e.g., 300nm-800nm) or a smaller increment values (e.g, 1nm)
    if ( rgbsen.size() != 81 )
    {
        fprintf(
            stderr,
            "Please double check the Camera "
            "Sensitivity data (e.g. the increment "
            "should be uniform from 380nm to 780nm).\n" );
        exit( -1 );
    }

    _spstMaxCol = max_element( max.begin(), max.end() ) - max.begin();
    setSensitivity( rgbsen );

    return 1;
}

//	=====================================================================
//	Fetch the sensitivity data of the camera (reading from the file)
//
//	inputs:
//      N/A
//
//	outputs:
//		vector <RGBSen>: the sensitivity (in vector) of the camera

vector<RGBSen> Spst::getSensitivity()
{
    return _rgbsen;
}

//	=====================================================================
//	Set the brand of camera
//
//	inputs:
//      const char *: brand (read from the file or
//                    the meta-data from libraw)
//
//	outputs:
//		void: _brand (private member)

void Spst::setBrand( const char *brand )
{
    assert( brand != nullptr );
    size_t len = strlen( brand );

    assert( len < 64 );

    if ( len > 64 )
        len = 64;

    _brand = (char *)malloc( len + 1 );
    memset( _brand, 0x0, len );
    memcpy( _brand, brand, len );
    _brand[len] = '\0';

    return;
}

//	=====================================================================
//	Set the model of camera
//
//	inputs:
//      const char *: model (read from the file or
//                    the meta-data from libraw)
//
//	outputs:
//		void: _brand (private member)

void Spst::setModel( const char *model )
{
    assert( model != nullptr );
    size_t len = strlen( model );

    assert( len < 64 );

    if ( len > 64 )
        len = 64;

    _model = (char *)malloc( len + 1 );
    memset( _model, 0x0, len );
    memcpy( _model, model, len );
    _model[len] = '\0';

    return;
}

//	=====================================================================
//	Set the wavelength increment value of the camera sensitivity
//
//	inputs:
//      uint8_t: inc (read from the file)
//
//	outputs:
//		void: _increment (private member)

void Spst::setWLIncrement( const int &inc )
{
    _increment = inc;

    return;
}

//	=====================================================================
//	Set the sensitivity data of the camera (reading from the file)
//
//	inputs:
//      const vector<RGBSen>: rgbsen (read from the file)
//
//	outputs:
//		void: _rgbsen (private member)

void Spst::setSensitivity( const vector<RGBSen> &rgbsen )
{
    _rgbsen = rgbsen;

    return;
}

Idt::Idt()
{
    _verbosity = 0;

    FORI( 81 )
    {
        _trainingSpec.push_back( trainSpec() );
        _cmf.push_back( CMF() );
    }

    // Init CMF data
    for (int j = 0, i = 380; i <= 780; i+=5, j+=1){
        _cmf[j]._wl = i;
        _cmf[j]._xbar = cmf_1931_data[j*15];
        _cmf[j]._ybar = cmf_1931_data[j*15+1];
        _cmf[j]._zbar = cmf_1931_data[j*15+2];
    }

    _idt.resize( 3 );
    _wb.resize( 3 );
    FORI( 3 )
    {
        _idt[i].resize( 3 );
        _wb[i]               = 1.0;
        FORJ( 3 ) _idt[i][j] = neutral3[i][j];
    }
}

Idt::~Idt()
{
    vector<Illum>().swap( _Illuminants );
    vector<CMF>().swap( _cmf );
    vector<trainSpec>().swap( _trainingSpec );
    vector<double>().swap( _wb );
    vector<vector<double>>().swap( _idt );
}

//	=====================================================================
//	Scale the Illuminant data using the max element of RGB code values
//
//	inputs:
//		Illum & Illuminant
//
//	outputs:
//		scaled Illuminant data set

void Idt::scaleLSC( Illum &Illuminant )
{
    assert( _cameraSpst._spstMaxCol >= 0 && ( Illuminant._data ).size() != 0 );

    int            size = _cameraSpst._rgbsen.size();
    vector<double> colMax( size, 1.0 );
    switch ( _cameraSpst._spstMaxCol )
    {
        case 0: FORI( size ) colMax[i] = _cameraSpst._rgbsen[i]._RSen; break;
        case 1: FORI( size ) colMax[i] = _cameraSpst._rgbsen[i]._GSen; break;
        case 2: FORI( size ) colMax[i] = _cameraSpst._rgbsen[i]._BSen; break;
        default: return;
    }

    scaleVector(
        Illuminant._data,
        1.0 / sumVector( mulVectorElement( Illuminant._data, colMax ) ) );
}

//	=====================================================================
//	Load the Camera Sensitivty data
//
//	inputs:
//		string: path to the camera sensitivity file
//      const char *: camera maker  (from libraw)
//      const char *: camera model  (from libraw)
//
//	outputs:
//		boolean: If successufully parsed, _cameraSpst will be filled and return 1;
//               Otherwise, return 0

int Idt::loadCameraSpst(
    const string &path, const char *maker, const char *model )
{

    return _cameraSpst.loadSpst( path, maker, model );
}

//	=====================================================================
//	Load the Illuminant data
//
//	inputs:
//		string: paths to various Illuminant data files
//      string: type of light source if user specifies
//
//	outputs:
//		int: If successufully parsed, _bestIllum will be filled and return 1;
//               Otherwise, return 0

int Idt::loadIlluminant( const vector<string> &paths, string type )
{
    //        assert ( paths.size() > 0 && !type.empty() );

    if ( _Illuminants.size() > 0 )
        _Illuminants.clear();

    if ( type.compare( "na" ) != 0 )
    {

        // Daylight
        if ( type[0] == 'd' )
        {
            Illum illumDay;
            illumDay.setIllumType( type );
            illumDay.calDayLightSPD( atoi( type.substr( 1 ).c_str() ) );
            _Illuminants.push_back( illumDay );

            return 1;
        }
        // Blackbody
        else if ( type[type.length() - 1] == 'k' )
        {
            Illum illumBB;
            illumBB.setIllumType( type );
            illumBB.calBlackBodySPD(
                atoi( type.substr( 0, type.length() - 1 ).c_str() ) );
            _Illuminants.push_back( illumBB );

            return 1;
        }
        else
        {
            FORI( paths.size() )
            {
                Illum IllumJson;
                if ( IllumJson.readSPD( paths[i], type ) &&
                     type.compare( IllumJson._type ) == 0 )
                {
                    _Illuminants.push_back( IllumJson );

                    return 1;
                }
            }
        }
    }
    else
    {
        // Daylight - pre-calculate
        for ( int i = 4000; i <= 25000; i += 500 )
        {
            Illum illumDay;
            illumDay.setIllumType( "d" + ( to_string( i / 100 ) ) );
            illumDay.calDayLightSPD( i );

            _Illuminants.push_back( illumDay );
        }

        // Blackbody - pre-calculate
        for ( int i = 1500; i < 4000; i += 500 )
        {
            Illum illumBB;
            illumBB.setIllumType( ( to_string( i ) + "k" ) );
            illumBB.calBlackBodySPD( i );

            _Illuminants.push_back( illumBB );
        }

        if (paths.empty()){
            Illum IllumDefault;
            IllumDefault.loadDefaultSPD();
            _Illuminants.push_back(IllumDefault);
        }

        FORI( paths.size() )
        {
            Illum IllumJson;
            if ( IllumJson.readSPD( paths[i], type ) )
                _Illuminants.push_back( IllumJson );
        }

        if (_Illuminants.empty()){
            // Add default hard coded illuminant
            Illum IllumDefault;
            IllumDefault.loadDefaultSPD();
            _Illuminants.push_back(IllumDefault);
        }
    }

    return ( _Illuminants.size() > 0 );
}

//	=====================================================================
//	Load the 190-patch training data
//
//	inputs:
//		string : path to the 190-patch training data
//
//	outputs:
//		_trainingSpec: If successufully parsed, _trainingSpec will be filled

void Idt::loadTrainingData( const string &path )
{
    struct stat st;
    //assert( !stat( path.c_str(), &st ) );

    if ( _trainingSpec.size() > 0 )
    {
        FORI( _trainingSpec.size() )
        _trainingSpec[i]._data.clear();
    }

    try
    {
        ptree pt;
        read_json( path, pt );

        int i = 0;

        BOOST_FOREACH (
            ptree::value_type &row, pt.get_child( "spectral_data.data.main" ) )
        {
            _trainingSpec[i]._wl = atoi( ( row.first ).c_str() );

            BOOST_FOREACH ( ptree::value_type &cell, row.second )
                _trainingSpec[i]._data.push_back(
                    cell.second.get_value<double>() );

            assert( _trainingSpec[i]._data.size() == 190 );

            i += 1;
        }
    }
    catch ( std::exception const &e )
    {
        std::cerr << e.what() << std::endl;
    }
}

//	=====================================================================
//	Push new Illuminant to further process Spectral Power Data
//
//	inputs:
//      Illum: Illuminant
//
//	outputs:
//		N/A:   _Illuminants should have one more element

void Idt::setIlluminants( const Illum &Illuminant )
{
    _Illuminants.push_back( Illuminant );
}

//	=====================================================================
//	Set Verbosity value for the length of IDT generation status message
//
//	inputs:
//      int: verbosity
//
//	outputs:
//		int: _verbosity

void Idt::setVerbosity( const int verbosity )
{
    _verbosity = verbosity;
}

//	=====================================================================
//	Choose the best Light Source based on White Balance Coefficients from
//  the camera read by libraw according to a given set of coefficients
//
//	inputs:
//		Map: Key: path to the Light Source data;
//           Value: Light Source x Camera Sensitivity
//      Vector: White Balance Coefficients
//
//	outputs:
//		Illum: the best _Illuminant

void Idt::chooseIllumSrc( const vector<double> &src, int highlight )
{
    double sse = dmax;

    FORI( _Illuminants.size() )
    {
        vector<double> wb_tmp  = calWB( _Illuminants[i], highlight );
        double         sse_tmp = calSSE( wb_tmp, src );

        if ( sse_tmp < sse )
        {
            sse        = sse_tmp;
            _bestIllum = _Illuminants[i];
            _wb        = wb_tmp;
        }
    }

    if ( _verbosity > 1 )
        printf(
            "The illuminant calculated to be the best match to the camera metadata is %s\n",
            _bestIllum._type.c_str() );

    // scale back the WB factor
    double factor = _wb[1];
    assert( factor != 0.0 );
    FORI( _wb.size() ) _wb[i] /= factor;

    return;
}

//	=====================================================================
//	Choose the best Light Source based on White Balance Coefficients from
//  the camera read by libraw according to user-specified illuminant
//
//	inputs:
//		Map: Key: path to the Light Source data;
//           Value: Light Source x Camera Sensitivity
//      String: Light Source Name
//
//	outputs:
//		Illum: the best _Illuminant

void Idt::chooseIllumType( const char *type, int highlight )
{
    assert( cmp_str( type, _Illuminants[0]._type.c_str() ) == 0 );

    _bestIllum = _Illuminants[0];
    _wb        = calWB( _bestIllum, highlight );

    //		if (_verbosity > 1)
    //            printf ( "The specified light source is: %s\n",
    //                     _bestIllum._type.c_str() );

    // scale back the WB factor
    double factor = _wb[1];
    assert( factor != 0.0 );
    FORI( _wb.size() ) _wb[i] /= factor;

    return;
}

//	=====================================================================
//	Calculate the middle product based on the camera sensitivity data
//  and Illuminant/light source data
//
//	inputs:
//		N/A
//
//	outputs:
//		vector < double >: scaled vector by its maximum value

vector<double> Idt::calCM()
{
    vector<RGBSen>         rgbsen = _cameraSpst.getSensitivity();
    vector<vector<double>> rgbsenV( 3, vector<double>( rgbsen.size(), 1.0 ) );

    FORI( rgbsen.size() )
    {
        rgbsenV[0][i] = rgbsen[i]._RSen;
        rgbsenV[1][i] = rgbsen[i]._GSen;
        rgbsenV[2][i] = rgbsen[i]._BSen;
    }

    vector<double> CM = mulVector( rgbsenV, _bestIllum._data );
    scaleVectorD( CM );

    return CM;
}

//	=====================================================================
//	Calculate the middle product based on the 190 patch / training data
//  and Illuminant/light source data
//
//	inputs:
//		N/A
//
//	outputs:
//		vector < vector<double> >: 2D vector (81 x 190)

vector<vector<double>> Idt::calTI() const
{
    assert(
        _bestIllum._data.size() == 81 && _trainingSpec[0]._data.size() == 190 );

    vector<vector<double>> TI( _bestIllum._data.size(), vector<double>( 190 ) );
    FORIJ( _bestIllum._data.size(), _trainingSpec[0]._data.size() )
    TI[i][j] = _bestIllum._data[i] * ( _trainingSpec[i]._data )[j];

    return TI;
}

//	=====================================================================
//	Calculate White Balance based on the Illuminant data and
//  highlight mode used in pre-processing with "libraw"
//
//	inputs:
//      Illum: Illuminant
//      int: highlight
//
//	outputs:
//		vector: wb(R, G, B)

vector<double> Idt::calWB( Illum &Illuminant, int highlight )
{
    assert( Illuminant._data.size() == 81 && _cameraSpst._rgbsen.size() > 0 );

    scaleLSC( Illuminant );

    vector<vector<double>> colRGB(3, vector<double>( Illuminant._data.size(), 1.0 ) );

    FORI( Illuminant._data.size() )
    {
        colRGB[0][i] = _cameraSpst._rgbsen[i]._RSen;
        colRGB[1][i] = _cameraSpst._rgbsen[i]._GSen;
        colRGB[2][i] = _cameraSpst._rgbsen[i]._BSen;
    }

    vector<double> wb = mulVector( colRGB, Illuminant._data );
    clearVM( colRGB );

    FORI( wb.size() ) wb[i] = invertD( wb[i] );

    if ( !highlight )
        scaleVectorMin( wb );
    else
        scaleVectorMax( wb );

    return wb;
}

//	=====================================================================
//	Calculate CIE XYZ tristimulus values of scene adopted white
//  based on training color spectral radiances from CalTI() and color
//  adaptation matrix from CalCAT()
//
//	inputs:
//		vector< vector<double> > outcome of CalTI()
//
//	outputs:
//		vector < vector<double> >: 2D vector (190 x 3)

vector<vector<double>> Idt::calXYZ( const vector<vector<double>> &TI ) const
{
    assert( TI.size() == 81 );

    vector<vector<double>> transTI = transposeVec( TI );
    vector<vector<double>> colXYZ( 3, vector<double>( TI.size(), 1.0 ) );

    FORI( TI.size() )
    {
        colXYZ[0][i] = _cmf[i]._xbar;
        colXYZ[1][i] = _cmf[i]._ybar;
        colXYZ[2][i] = _cmf[i]._zbar;
    }

    vector<vector<double>> XYZ = transposeVec( mulVector( colXYZ, transTI ) );

    FORI( XYZ.size() )
    scaleVector(
        XYZ[i],
        1.0 / sumVector( mulVectorElement( colXYZ[1], _bestIllum._data ) ) );

    vector<double> ww = mulVector( colXYZ, _bestIllum._data );
    scaleVector( ww, ( 1.0 / ww[1] ) );
    vector<double> w( XYZ_w, XYZ_w + 3 );

    XYZ = mulVector( XYZ, getCAT( ww, w ) );

    clearVM( ww );
    clearVM( w );

    return XYZ;
}

//	=====================================================================
//	Calculate white-balanced linearized camera system response (in RGB)
//  based on training color spectral radiances from CalTI() and white
//  balance factors from calWB(...)
//
//	inputs:
//		vector< vector<double> > outcome of CalTI()
//
//	outputs:
//		vector < vector<double> >: 2D vector (190 x 3)

vector<vector<double>> Idt::calRGB( const vector<vector<double>> &TI ) const
{
    assert( TI.size() == 81 );

    vector<vector<double>> transTI = transposeVec( TI );
    vector<vector<double>> colRGB( 3, vector<double>( TI.size(), 1.0 ) );

    FORI( TI.size() )
    {
        colRGB[0][i] = _cameraSpst._rgbsen[i]._RSen;
        colRGB[1][i] = _cameraSpst._rgbsen[i]._GSen;
        colRGB[2][i] = _cameraSpst._rgbsen[i]._BSen;
    }

    vector<vector<double>> RGB = mulVector( transTI, colRGB );

    FORI( RGB.size() )
    RGB[i] = mulVectorElement( _wb, RGB[i] );

    clearVM( transTI );
    clearVM( colRGB );

    return RGB;
}

//	=====================================================================
//	Process cureve fit between XYZ and RGB data with initial set of B
//  values.
//
//	inputs:
//		vector< vector<double> >: RGB
//      vector< vector<double> >: XYZ
//      double * :                B (6 elements)
//
//	outputs:
//      boolean: if succeed, _idt should be filled with values
//               that minimize the distance between RGB and XYZ
//               through updated B.

struct Objfun
{
    Objfun(
        const vector<vector<double>> &RGB,
        const vector<vector<double>> &outLAB )
        : _RGB( RGB ), _outLAB( outLAB )
    {}

    template <typename T> bool operator()( const T *B, T *residuals ) const
    {
        vector<vector<T>> RGBJet( 190, vector<T>( 3 ) );
        FORIJ( 190, 3 ) RGBJet[i][j] = T( _RGB[i][j] );
    
        vector<vector<T>> outCalcLAB         = XYZtoLAB( getCalcXYZt( RGBJet, B ) );
        FORIJ( 190, 3 ) residuals[i * 3 + j] = _outLAB[i][j] - outCalcLAB[i][j];
    
        return true;
    }

    const vector<vector<double>> _RGB;
    const vector<vector<double>> _outLAB;
};

int Idt::curveFit(
    const vector<vector<double>> &RGB,
    const vector<vector<double>> &XYZ,
    double                       *B )
{
    ceres::Problem                problem;
    vector<vector<double>> outLAB = XYZtoLAB( XYZ );

    ceres::CostFunction *cost_function =
        new ceres::AutoDiffCostFunction<Objfun, ceres::DYNAMIC, 6>(
            new Objfun( RGB, outLAB ), int( RGB.size() * ( RGB[0].size() ) ) );

    problem.AddResidualBlock( cost_function, NULL, B );

    ceres::Solver::Options options;
    options.linear_solver_type  = ceres::DENSE_QR;
    options.parameter_tolerance = 1e-17;
    //        options.gradient_tolerance = 1e-17;
    options.function_tolerance        = 1e-17;
    options.min_line_search_step_size = 1e-17;
    options.max_num_iterations        = 300;

    if ( _verbosity > 2 )
        options.minimizer_progress_to_stdout = true;

    ceres::Solver::Summary summary;
    ceres::Solve( options, &problem, &summary );

    if ( _verbosity > 1 )
        std::cout << summary.BriefReport() << std::endl;
    else if ( _verbosity >= 2 )
        std::cout << summary.FullReport() << std::endl;

    if ( summary.num_successful_steps )
    {
        _idt[0][0] = B[0];
        _idt[0][1] = B[1];
        _idt[0][2] = 1.0 - B[0] - B[1];
        _idt[1][0] = B[2];
        _idt[1][1] = B[3];
        _idt[1][2] = 1.0 - B[2] - B[3];
        _idt[2][0] = B[4];
        _idt[2][1] = B[5];
        _idt[2][2] = 1.0 - B[4] - B[5];

        if ( _verbosity > 1 )
        {
            printf( "The IDT matrix is ...\n" );
            FORI( 3 )
            printf( "   %f %f %f\n", _idt[i][0], _idt[i][1], _idt[i][2] );
        }

        return 1;
    }

    delete cost_function;

    return 0;
}

//	=====================================================================
//	Calculate IDT matrix by calling curveFit(...)
//
//	inputs:
//         N/A
//
//	outputs: through curveFit(...)
//      boolean: if succeed, _idt should be filled with values
//               that minimize the distance between RGB and XYZ
//               through updated B.

int Idt::calIDT()
{

    double                 BStart[6] = { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
    vector<vector<double>> TI        = calTI();

    return curveFit( calRGB( TI ), calXYZ( TI ), BStart );
}

//	=====================================================================
//  Get camera sensitivity data that was loaded from the file
//
//	inputs:
//         N/A
//
//	outputs:
//      const Spst: camera sensitivity data that was loaded from the file

const Spst Idt::getCameraSpst() const
{
    return _cameraSpst;
}

//	=====================================================================
//  Get Illuminant data / light source that was loaded from the file
//
//	inputs:
//         N/A
//
//	outputs:
//      const vector < illum >: Illuminant data that was loaded from
//      the file

const vector<Illum> Idt::getIlluminants() const
{
    return _Illuminants;
}

//	=====================================================================
//  Get the Best Illuminant data / light source that was loaded from
//  the file
//
//	inputs:
//         N/A
//
//	outputs:
//      const illum: Illuminant data that has the closest match

const Illum Idt::getBestIllum() const
{
    assert( ( _bestIllum.getIllumData() ).size() != 0 );

    return _bestIllum;
}


//	=====================================================================
//  Get Spectral Training Data that was loaded from the file
//
//	inputs:
//         N/A
//
//	outputs:
//      const vector < trainSpec >: Spectral Training data that was loaded
//      from the file

const vector<trainSpec> Idt::getTrainingSpec() const
{
    return _trainingSpec;
}

//	=====================================================================
//  Get Color Matching Function Data that was loaded from the file
//
//	inputs:
//         N/A
//
//	outputs:
//      const CMF: Color Matching Function data that was loaded from
//      the file

const vector<CMF> Idt::getCMF() const
{
    return _cmf;
}

//	=====================================================================
//  Get Idt matrix if CalIDT() succeeds
//
//	inputs:
//         N/A
//
//	outputs:
//      const vector< vector < double > >: _idt matrix (3 x 3)

const vector<vector<double>> Idt::getIDT() const
{
    return _idt;
}

void Idt::getIdtF(float matrix[9]) const
{
    FORIJ ( 3, 3 ) matrix[i*3+j] = _idt[i][j];
}

//	=====================================================================
//  Get white balanced if calWB(...) succeeds
//
//	inputs:
//         N/A
//
//	outputs:
//      const vector< double >: _wb vector (1 x 3)

const vector<double> Idt::getWB() const
{
    return _wb;
}

void Idt::getWBF(float wb[3]) const
{
    FORI(3) wb[i] = _wb[i];
}

}// namespace