#pragma once

template <class T>
inline void matrix_vector_mult(const T *mat, const T *vec, T *result)
{
    for (int i = 0; i < 3; i++)
    {
        T res = 0.0;
        for (int j = 0; j < 3; j++)
        {
            res += mat[i * 3 + j] * vec[j];
        }
        result[i] = res;
    }
}

template <class T>
inline void mat_mat_mult(const T *mat1, const T *mat2, T *result)
{
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            T res = 0.0;
            for (int k = 0; k < 3; k++)
            {
                res += mat1[i * 3 + k] * mat2[k * 3 + j];
            }
            result[i * 3 + j] = res;
        }
    }
}

template <class T>
inline void invert_matrix(const T *mat, T *result)
{
    T det = mat[0] * (mat[4] * mat[8] - mat[5] * mat[7]) -
            mat[1] * (mat[3] * mat[8] - mat[5] * mat[6]) +
            mat[2] * (mat[3] * mat[7] - mat[4] * mat[6]);
    if (det == 0)
        return;
    det = 1.f / det;
    result[0] = (mat[4] * mat[8] - mat[5] * mat[7]) * det;
    result[1] = -(mat[1] * mat[8] - mat[2] * mat[7]) * det;
    result[2] = (mat[1] * mat[5] - mat[2] * mat[4]) * det;
    result[3] = -(mat[3] * mat[8] - mat[5] * mat[6]) * det;
    result[4] = (mat[0] * mat[8] - mat[2] * mat[6]) * det;
    result[5] = -(mat[0] * mat[5] - mat[2] * mat[3]) * det;
    result[6] = (mat[3] * mat[7] - mat[4] * mat[6]) * det;
    result[7] = -(mat[0] * mat[7] - mat[1] * mat[6]) * det;
    result[8] = (mat[0] * mat[4] - mat[1] * mat[3]) * det;
}

template <class T>
class Vector3
{
    T vec[3];

public:
    Vector3()
    {
        vec[0] = vec[1] = vec[2] = 0;
    }

    Vector3(const T &a, const T &b, const T &c)
    {
        vec[0] = a;
        vec[1] = b;
        vec[2] = c;
    }

    Vector3(const T *ref)
    {
        for (int i = 0; i < 3; ++i)
            vec[i] = ref[i];
    }

    Vector3(const Vector3 &ref)
    {
        for (int i = 0; i < 3; ++i)
            vec[i] = ref.vec[i];
    }

    void print(const char *prefix) const
    {
        printf("%s Vector3(%f, %f, %f)\n", prefix, vec[0], vec[1], vec[2]);
    }

    void copy_to(T *ref) const
    {
        for (int i = 0; i < 3; ++i)
            ref[i] = vec[i];
    }

    Vector3 operator+(const Vector3 &a) const
    {
        return Vector3(vec[0] + a.vec[0], vec[1] + a.vec[1], vec[2] + a.vec[2]);
    }

    Vector3 operator-(const Vector3 &a) const
    {
        return Vector3(vec[0] - a.vec[0], vec[1] - a.vec[1], vec[2] - a.vec[2]);
    }

    Vector3 operator*(const Vector3 &a) const
    {
        return Vector3(vec[0] * a.vec[0], vec[1] * a.vec[1], vec[2] * a.vec[2]);
    }

    Vector3 operator*(const T &a) const
    {
        return Vector3(vec[0] * a, vec[1] * a, vec[2] * a);
    }

    Vector3 operator/(const T &a) const
    {
        return Vector3(vec[0] / a, vec[1] / a, vec[2] / a);
    }

    Vector3 operator/(const Vector3 &a) const
    {
        return Vector3(vec[0] / a.vec[0], vec[1] / a.vec[1], vec[2] / a.vec[2]);
    }

    Vector3 operator-() const
    {
        return Vector3(-vec[0], -vec[1], -vec[2]);
    }

    Vector3& operator*=(const Vector3 &a)
    {
        vec[0] *= a.vec[0];
        vec[1] *= a.vec[1];
        vec[2] *= a.vec[2];
        return *this;
    }

    Vector3& operator=(const Vector3 &a)
    {
        vec[0] = a.vec[0];
        vec[1] = a.vec[1];
        vec[2] = a.vec[2];
        return *this;
    }

    Vector3 clip(const T &min, const T &max) const
    {
        return Vector3(std::max(min, std::min(max, vec[0])),
                       std::max(min, std::min(max, vec[1])),
                       std::max(min, std::min(max, vec[2])));
    }

    void clip_in_place(const T &min, const T &max)
    {
        vec[0] = std::max(min, std::min(max, vec[0]));
        vec[1] = std::max(min, std::min(max, vec[1]));
        vec[2] = std::max(min, std::min(max, vec[2]));
    }

    T min() const
    {
        return std::min(vec[0], std::min(vec[1], vec[2]));
    }

    T max() const
    {
        return std::max(vec[0], std::max(vec[1], vec[2]));
    }

    T *data()
    {
        return vec;
    }

    const T *data() const
    {
        return vec;
    }

    T &operator[](int i)
    {
        return vec[i];
    }

    T operator[](int i) const
    {
        return vec[i];
    }
};

template <class T>
class Vector2
{
    T vec[2];

public:
    Vector2()
    {
        vec[0] = vec[1] = 0;
    }

    Vector2(const T &a, const T &b)
    {
        vec[0] = a;
        vec[1] = b;
    }

    Vector2(const T *ref)
    {
        for (int i = 0; i < 2; ++i)
            vec[i] = ref[i];
    }

    Vector2(const Vector2 &ref)
    {
        for (int i = 0; i < 2; ++i)
            vec[i] = ref.vec[i];
    }

    T *data()
    {
        return vec;
    }

    T &operator[](int i)
    {
        return vec[i];
    }

    T operator[](int i) const
    {
        return vec[i];
    }

    Vector2& operator = (const Vector2& b)
    {
        vec[0] = b.vec[0];
        vec[1] = b.vec[1];
        return *this;
    }
};

template <class T>
class Matrix3x3
{
    T mat[9];

public:
    // Identity
    Matrix3x3()
    {
        identity();
    }

    Matrix3x3(const T *ref)
    {
        for (int i = 0; i < 9; ++i)
            mat[i] = ref[i];
    }

    Matrix3x3(const Matrix3x3 &ref)
    {
        for (int i = 0; i < 9; ++i)
            mat[i] = ref.mat[i];
    }

    Matrix3x3(const Vector3<T> &a, const Vector3<T> &b, const Vector3<T> &c)
    {
        mat[0] = a[0];
        mat[1] = a[1];
        mat[2] = a[2];
        mat[3] = b[0];
        mat[4] = b[1];
        mat[5] = b[2];
        mat[6] = c[0];
        mat[7] = c[1];
        mat[8] = c[2];
    }

    void transpose()
    {
        T temp;
        temp = mat[1]; mat[1] = mat[3]; mat[3] = temp;
        temp = mat[2]; mat[2] = mat[6]; mat[6] = temp;
        temp = mat[5]; mat[5] = mat[7]; mat[7] = temp;
    }

    Matrix3x3(const T &a, const T &b, const T &c,
              const T &d, const T &e, const T &f,
              const T &g, const T &h, const T &i)
    {
        mat[0] = a;mat[1] = b;mat[2] = c;
        mat[3] = d;mat[4] = e;mat[5] = f;
        mat[6] = g;mat[7] = h;mat[8] = i;
    }

    void identity()
    {
        mat[0] = 1;mat[1] = 0;mat[2] = 0;
        mat[3] = 0;mat[4] = 1;mat[5] = 0;
        mat[6] = 0;mat[7] = 0;mat[8] = 1;
    }

    void print(const char *prefix) const
    {
        printf("%s Matrix3x3(%f, %f, %f, %f, %f, %f, %f, %f, %f)\n",
               prefix, mat[0], mat[1], mat[2],
               mat[3], mat[4], mat[5],
               mat[6], mat[7], mat[8]);
    }

    T *data()
    {
        return mat;
    }

    T &operator[](int i)
    {
        return mat[i];
    }

    Matrix3x3 operator*(const Matrix3x3 &a) const
    {
        Matrix3x3 res;
        mat_mat_mult(mat, a.mat, res.mat);
        return res;
    }

    Vector3<T> operator*(const Vector3<T> &a) const
    {
      Vector3<T> res;
      matrix_vector_mult(mat, a.data(), res.data());
      return res;
    }

    Matrix3x3 scale(const Vector3<T> &a) const
    {
        Matrix3x3 res;
        res.mat[0] = mat[0] * a[0];
        res.mat[1] = mat[1] * a[1];
        res.mat[2] = mat[2] * a[2];
        res.mat[3] = mat[3] * a[0];
        res.mat[4] = mat[4] * a[1];
        res.mat[5] = mat[5] * a[2];
        res.mat[6] = mat[6] * a[0];
        res.mat[7] = mat[7] * a[1];
        res.mat[8] = mat[8] * a[2];
        return res;
    }

    Matrix3x3& operator=(const Matrix3x3 &a)
    {
        for (int i = 0; i < 9; ++i)
            mat[i] = a.mat[i];
        return *this;
    }

    Matrix3x3 invert() const
    {
        Matrix3x3 res;
        invert_matrix(mat, res.mat);
        return res;
    }

    void invert_in_place()
    {
        Matrix3x3 res;
        invert_matrix(mat, res.mat);
        for (int i = 0; i < 9; ++i)
            mat[i] = res[i];
    }

    void normalize_rows()
    {
        float sum[3] = {mat[0] + mat[1] + mat[2],
                        mat[3] + mat[4] + mat[5],
                        mat[6] + mat[7] + mat[8]};
        // Normalize rows
        mat[0] /= sum[0];
        mat[1] /= sum[0];
        mat[2] /= sum[0];
        mat[3] /= sum[1];
        mat[4] /= sum[1];
        mat[5] /= sum[1];
        mat[6] /= sum[2];
        mat[7] /= sum[2];
        mat[8] /= sum[2];
    }
};

// Color primaries in xy format
template <class T>
class PrimariesXY : public Vector2<T>
{
public:
    PrimariesXY(T x, T y) : Vector2<T>(x, y) {}

    PrimariesXY(const Vector2<T> &v) : Vector2<T>(v) {}

    PrimariesXY() : Vector2<T>() {}

    Vector3<T> to_XYZ() const
    {
        // Convert xy to XYZ using the formula:
        // X = x / y, Y = 1, Z = (1 - x - y) / y
        return Vector3<T>(X() / Y(), 1., (1. - X() - Y()) / Y());
    }

    T X() const
    {
        return (*this)[0];
    }

    T Y() const
    {
        return (*this)[1];
    }
};

// Typedefs

typedef Matrix3x3<float> Matrix3x3f;
typedef Vector3<float> Vector3f;
typedef Vector2<float> Vector2f;
typedef PrimariesXY<float> PrimariesXYf;

// Predefined white point primaries

template <class T>
const PrimariesXY<T> WP_ACES = PrimariesXY<T>(0.32168, 0.33767);
template <class T>
const PrimariesXY<T> WP_D65 = PrimariesXY<T>(0.31272, 0.32903);
template <class T>
const PrimariesXY<T> WP_D50 = PrimariesXY<T> (0.34567, 0.35850);
template <class T>
const PrimariesXY<T> WP_P3_DCI = PrimariesXY<T> (0.314, 0.351);
template <class T>
const PrimariesXY<T> WP_DISPLAY_DREAMCOLOR = PrimariesXY<T> (0.303, 0.317);

// Chromatic adaptation matrices

template <class T>
const Matrix3x3<T> bradford_matrix(
    0.8951, 0.2664, -0.1614,
    -0.7502, 1.7135, 0.0367,
    0.0389, -0.0685, 1.0296);

template <class T>
const Matrix3x3<T> cmccat2000_matrix(
    0.7982,  0.3389, -0.1371,
    -0.5918,  1.5512,  0.0406,
    0.0008,  0.0239,  0.9753);

template <class T>
const Matrix3x3<T> ciecat02_matrix(
    0.7328,  0.4296, -0.1624,
    -0.7036,  1.6975,  0.0061,
    0.0030,  0.0136,  0.9834);

// RGB primaries in xy format
template <class T>
class RGBPrimaries
{
    PrimariesXY<T> red;
    PrimariesXY<T> green;
    PrimariesXY<T> blue;
    PrimariesXY<T> white;
public:
    RGBPrimaries(T r_x, T r_y, T g_x, T g_y, T b_x, T b_y, T W_x, T W_y)
    {
        red = PrimariesXY<T>(r_x, r_y);
        green = PrimariesXY<T>(g_x, g_y);
        blue = PrimariesXY<T>(b_x, b_y);
        white = PrimariesXY<T>(W_x, W_y);
    }

    RGBPrimaries(T r_x, T r_y, T g_x, T g_y, T b_x, T b_y, const PrimariesXY<T>& w)
    {
        red = PrimariesXY<T>(r_x, r_y);
        green = PrimariesXY<T>(g_x, g_y);
        blue = PrimariesXY<T>(b_x, b_y);
        white = w;
    }

    RGBPrimaries(const PrimariesXY<T> &r, const PrimariesXY<T> &g, const PrimariesXY<T> &b, const PrimariesXY<T>& w)
    {
        red = r;
        green = g;
        blue = b;
        white = w;
    }

    PrimariesXY<T> red_primaries() const
    {
        return red;
    }

    PrimariesXY<T> green_primaries() const
    {
        return green;
    }

    PrimariesXY<T> blue_primaries() const
    {
        return blue;
    }

    PrimariesXY<T> white_primaries() const
    {
        return white;
    }

    Matrix3x3<T> toRgbMatrix() const
    {
        Vector3<T> X = red.to_XYZ();
        Vector3<T> Y = green.to_XYZ();
        Vector3<T> Z = blue.to_XYZ();
        Matrix3x3<T> ret(X, Y, Z);
        ret.transpose();
        return ret;
    }

    Matrix3x3<T> compute_adaptation_matrix(const PrimariesXY<T> source_whitepoint,
                                           const PrimariesXY<T> target_whitepoint,
                                        const Matrix3x3<T>& ca_matrix = bradford_matrix<T>) const
    {
        Vector3<T> source_whitepoint_XYZ = source_whitepoint.to_XYZ();
        Vector3<T> target_whitepoint_XYZ = target_whitepoint.to_XYZ();

        Vector3<T> chromatic_adapt_source = ca_matrix * source_whitepoint_XYZ;
        Vector3<T> chromatic_adapt_target = ca_matrix * target_whitepoint_XYZ;

        Matrix3x3<T> chromatic_adaptation_matrix = Matrix3x3<T>(
            chromatic_adapt_target[0] / chromatic_adapt_source[0], 0, 0,
            0, chromatic_adapt_target[1] / chromatic_adapt_source[1], 0,
            0, 0, chromatic_adapt_target[2] / chromatic_adapt_source[2]);

        return ca_matrix.invert() * chromatic_adaptation_matrix;
    }

    // Compute RGBW primaries to XYZ using a chromatic adaptation matrix, the resulting matrix will convert RGB->XYZ
    // If XYZ->RGB Matrix is needed, just set invert to true
    Matrix3x3<T> compute_adapted_matrix(bool invert = false,
                                        const PrimariesXY<T> target_whitepoint = WP_D50<T>,
                                        const Matrix3x3<T>& ca_matrix = bradford_matrix<float>) const
    {
        Matrix3x3<T> chromatic_adaptation_matrix = compute_adaptation_matrix(white, target_whitepoint, ca_matrix);

        Matrix3x3<T> rgb_primaries = toRgbMatrix();
        Vector3<T> S = rgb_primaries.invert() * white.to_XYZ();
        Matrix3x3<T> to_xyz = rgb_primaries.scale(S);

        Matrix3x3<T> source_target_white_matrix = (chromatic_adaptation_matrix) * ca_matrix;
        Matrix3x3<T> rgb_to_xyz = source_target_white_matrix * to_xyz;
    
        return invert ? rgb_to_xyz.invert() : rgb_to_xyz;
    }


};

typedef RGBPrimaries<float> RGBPrimariesf;

// Predefined RGB primaries

template <class T>
const RGBPrimaries<T> rec709d65_primaries = RGBPrimaries<T>(0.640, 0.330,
                                                            0.300, 0.600,
                                                            0.150, 0.006, 
                                                            WP_D65<T>); // D65 white point

template <class T>
const RGBPrimaries<T> rec709d50_primaries = RGBPrimaries<T>(0.640, 0.330,
                                                            0.300, 0.600,
                                                            0.150, 0.006, 
                                                            WP_D50<T>); // D50 white point

template <class T>                                                      
const RGBPrimaries<T>  rec2020d65_primaries = RGBPrimaries<T> (0.708, 0.292,
                                                               0.170, 0.797,
                                                               0.131, 0.046,
                                                               WP_D65<T>);

template <class T>
const RGBPrimaries<T>  p3_display_primaries = RGBPrimaries<T> (0.680, 0.320,
                                                               0.265, 0.690,
                                                               0.150, 0.060,
                                                               WP_D65<T>);

template <class T>
const RGBPrimaries<T>  dci_p3_primaries = RGBPrimaries<T> (0.680, 0.320,
                                                           0.265, 0.690,
                                                           0.150, 0.060,
                                                           WP_P3_DCI<T>);

template <class T>
const RGBPrimaries<T>  bt601_primaries = RGBPrimaries<T> (0.640, 0.330,
                                                          0.290, 0.600,
                                                          0.150, 0.060,
                                                          WP_D65<T>);

template <class T>
const RGBPrimaries<T>  wide_gamut_rgb_primaries = RGBPrimaries<T> (0.7347, 0.2653,
                                                                  0.1152, 0.8264,
                                                                  0.1566, 0.0177,
                                                                  WP_D50<T>);

template <class T>
const RGBPrimaries<T>  hp_dreamcolor_rgb_primaries = RGBPrimaries<T> (0.684, 0.313,
                                                                      0.212, 0.722,
                                                                      0.149, 0.054,
                                                                      WP_DISPLAY_DREAMCOLOR<T>);

template <class T>
const RGBPrimaries<T>  adobe_rgb_primaries = RGBPrimaries<T> (0.640, 0.330,
                                                              0.210, 0.710,
                                                              0.150, 0.060,
                                                              WP_D65<T>);

template <class T>
const RGBPrimaries<T>  ap0_primaries = RGBPrimaries<T> (0.7347, 0.2653,
                                                        0.000, 1.000,
                                                        0.001, -0.077,
                                                        WP_ACES<T>);                                                              

template <class T>
const RGBPrimaries<T>  ap1_primaries = RGBPrimaries<T> (0.713, 0.293,
                                                        0.165, 0.830,
                                                        0.128, 0.044,
                                                        WP_ACES<T>); 

template <class T>
inline Matrix3x3<T> rec709_to_xyzD65_matrix(const Matrix3x3<T> &ca_matrix = bradford_matrix<T>)
{
    // Convert Rec709 to XYZ D65 using the Bradford chromatic adaptation matrix
    return rec709d65_primaries<T>.compute_adapted_matrix(
        false,
        WP_D65<T>,
        ca_matrix);
}

template <class T>
inline Matrix3x3<T> rec709_to_xyzD50_matrix(const Matrix3x3<T> &ca_matrix = bradford_matrix<T>)
{
    // Convert Rec709 to XYZ D50 using the Bradford chromatic adaptation matrix
    return rec709d65_primaries<T>.compute_adapted_matrix(
        false,
        WP_D50<T>,
        ca_matrix);
}

