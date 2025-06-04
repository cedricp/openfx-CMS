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

    Vector3 operator*=(const Vector3 &a)
    {
        vec[0] *= a.vec[0];
        vec[1] *= a.vec[1];
        vec[2] *= a.vec[2];
        return *this;
    }

    Vector3 clip(const T &min, const T &max) const
    {
        return Vector3(std::max(min, std::min(max, vec[0])),
                       std::max(min, std::min(max, vec[1])),
                       std::max(min, std::min(max, vec[2])));
    }

    T min() const
    {
        return std::min(vec[0], std::min(vec[1], vec[2]));
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
        mat[0] = a;
        mat[1] = b;
        mat[2] = c;
        mat[3] = d;
        mat[4] = e;
        mat[5] = f;
        mat[6] = g;
        mat[7] = h;
        mat[8] = i;
    }

    void identity()
    {
        mat[0] = 1;
        mat[1] = 0;
        mat[2] = 0;
        mat[3] = 0;
        mat[4] = 1;
        mat[5] = 0;
        mat[6] = 0;
        mat[7] = 0;
        mat[8] = 1;
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

    // Vector3<T> operator*(const Vector3<T> &a) const
    // {
    //   Vector3<T> res;
    //   matrix_vector_mult(mat, a.data(), res.data());
    //   return res;
    // }

    Matrix3x3 operator*(const Vector3<T> &a) const
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

    Vector3<T> vecmult(const Vector3<T> &a) const
    {
        Vector3<T> res;
        matrix_vector_mult(mat, a.data(), res.data());
        return res;
    }

    void operator=(const Matrix3x3 &a)
    {
        for (int i = 0; i < 9; ++i)
            mat[i] = a.mat[i];
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

typedef Matrix3x3<float> Matrix3x3f;
typedef Vector3<float> Vector3f;
typedef Vector2<float> Vector2f;

const Matrix3x3f xyzD65_rec709D65(
    3.2404542, -1.5371385, -0.4985314,
    -0.9692660, 1.8760108, 0.0415560,
    0.0556434, -0.2040259, 1.0572252);

const Matrix3x3f rec709toxyzD65(
    0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041);

const Matrix3x3f rec709toxyzD50(
    0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041);

const Matrix3x3f xyzD50toxyxD65(
    0.9555766, -0.0230393, 0.0631636,
    -0.0282895, 1.0099416, 0.0210077,
    0.0122982, -0.0204830, 1.3299098);

const Matrix3x3f xyzD65toxyxD50(
    1.0478112, 0.0228866, -0.0501270,
    0.0295424, 0.9904844, -0.0170491,
    -0.0092345, 0.0150436, 0.7521316);

const Matrix3x3f bradford_matrix(
    0.8951, 0.2664, -0.1614,
    -0.7502, 1.7135, 0.0367,
    0.0389, -0.0685, 1.0296);

template <class T>
inline Matrix3x3<T> get_matrix_cam2rec709(const Matrix3x3<T> &xyzD65tocam)
{
    Matrix3x3f rgb2cam;
    // RGB2CAM =  XYZTOCAMRGB(colormatrix) * REC709TOXYZ
    rgb2cam = xyzD65tocam * rec709toxyzD65;
    rgb2cam.normalize_rows();
    // Invert RGB2CAM matrix
    return rgb2cam.invert();
}

template <class T>
class PrimaryXY : public Vector2<T>
{
public:
    PrimaryXY(T x, T y) : Vector2<T>(x, y) {}

    PrimaryXY(const Vector2<T> &v) : Vector2<T>(v) {}

    PrimaryXY() : Vector2<T>() {}

    Vector3<T> to_XYZ() const
    {
        // Convert xy to XYZ using the formula:
        // X = x / y, Y = 1, Z = (1 - x - y) / y
        return Vector3<T>(X() / Y(), 1., (1. - X() - Y()) / Y());
    }

    float X() const
    {
        return (*this)[0];
    }

    float Y() const
    {
        return (*this)[1];
    }
};

template <class T>
class Primaries
{
    PrimaryXY<T> red;
    PrimaryXY<T> green;
    PrimaryXY<T> blue;
public:
    Primaries(T r_x, T r_y, T g_x, T g_y, T b_x, T b_y)
    {
        red = PrimaryXY<T>(r_x, r_y);
        green = PrimaryXY<T>(g_x, g_y);
        blue = PrimaryXY<T>(b_x, b_y);
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
};

template <class T>
inline Matrix3x3<T> compute_adapted_matrix(const Primaries<T> &primaries, PrimaryXY<T> source_whitepoint, PrimaryXY<T> target_whitepoint, bool invert = false)
{
    Vector3<T> source_whitepoint_XYZ = source_whitepoint.to_XYZ();
    Vector3<T> target_whitepoint_XYZ = target_whitepoint.to_XYZ();
    Matrix3x3<T> rgb_primaries = primaries.toRgbMatrix();
    Vector3<T> S = rgb_primaries.invert().vecmult(source_whitepoint_XYZ);

    Matrix3x3<T> to_xyz = rgb_primaries * S;

    Vector3<T> chromatic_adapt_source = bradford_matrix.vecmult(source_whitepoint_XYZ);
    Vector3<T> chromatic_adapt_target = bradford_matrix.vecmult(target_whitepoint_XYZ);

    Matrix3x3<T> chromatic_adaptation_matrix = Matrix3x3<T>(
        chromatic_adapt_target[0] / chromatic_adapt_source[0], 0, 0,
        0, chromatic_adapt_target[1] / chromatic_adapt_source[1], 0,
        0, 0, chromatic_adapt_target[2] / chromatic_adapt_source[2]);

    Matrix3x3<T> source_target_white_matrix = (bradford_matrix.invert() * chromatic_adaptation_matrix) * bradford_matrix;
    Matrix3x3<T> rgb_to_xyz = source_target_white_matrix * to_xyz;

    if (invert)
    {
        rgb_to_xyz.invert_in_place();
    }

    return rgb_to_xyz;
}
