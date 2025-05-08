#pragma once


const float xyzD65_rec709D65[9] = {
    3.2404542, -1.5371385, -0.4985314,
    -0.9692660 , 1.8760108, 0.0415560,
     0.0556434, -0.2040259, 1.0572252
};

const float rec709toxyzD65[9] = {
    0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041
};

const float rec709toxyzD50[9] = {
    0.4124564,  0.3575761,  0.1804375,
    0.2126729,  0.7151522,  0.0721750,
    0.0193339,  0.1191920,  0.9503041
};

const float xyzD50toxyxD65[9] = {
    0.9555766, -0.0230393,  0.0631636,
    -0.0282895,  1.0099416,  0.0210077,
     0.0122982, -0.0204830,  1.3299098
};

const float xyzD65toxyxD50[9] = {
    1.0478112,  0.0228866, -0.0501270,
    0.0295424,  0.9904844, -0.0170491,
    -0.0092345,  0.0150436,  0.7521316
};

template <class T>
inline void matrix_vector_mult(const T *mat, const T *vec, T *result)
{
    for (int i = 0; i < 3; i++)
    {
        T res = 0.0;
        for (int j = 0; j < 3; j++)
        {
            res += mat[i*3+j] * vec[j];
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
                res += mat1[i*3+k] * mat2[k*3+j];
            }
            result[i*3+j] = res;
        }
    }
}

template <class T>
inline void invert_matrix(const T *mat, T *result)
{
    T det = mat[0] * (mat[4] * mat[8] - mat[5] * mat[7]) -
                mat[1] * (mat[3] * mat[8] - mat[5] * mat[6]) +
                mat[2] * (mat[3] * mat[7] - mat[4] * mat[6]);
    if (det == 0) return;
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
class Matrix3x3
{
  T mat[9];
  public:

  // Identity
  Matrix3x3(){
    mat[0] = 1; mat[1] = 0; mat[2] = 0;
    mat[3] = 0; mat[4] = 0; mat[5] = 0;
    mat[6] = 0; mat[7] = 0; mat[8] = 1;
  }

  Matrix3x3(const T* ref){
    for(int i = 0; i < 9; ++i) mat[i] = ref[i];
  }

  Matrix3x3(const Matrix3x3& ref){
    for(int i = 0; i < 9; ++i) mat[i] = ref.mat[i];
  }

  T* data(){
    return mat;
  }

  T& operator[] (int i){
    return mat[i];
  }

  Matrix3x3 operator * (const Matrix3x3& a) const {
    Matrix3x3 res;
    mat_mat_mult(mat, a.mat, res.mat);
    return res;
  }

  void operator = (const Matrix3x3& a) {
    for(int i = 0; i < 9; ++i) mat[i] = a.mat[i];
  }

  Matrix3x3 invert() const {
    Matrix3x3 res;
    invert_matrix(mat, res.mat);
    return res;
  }

  void invert_in_place(){
    Matrix3x3 res;
    invert_matrix(mat, res.mat);
    for(int i = 0; i < 9; ++i) mat[i] = res[i];
  }

  void normalize_rows(){
    float sum[3] = {mat[0] + mat[1] + mat[2],
                    mat[3] + mat[4] + mat[5],
                    mat[6] + mat[7] + mat[8]};
    // Normalize rows
    mat[0] /= sum[0];mat[1] /= sum[0];mat[2] /= sum[0];
    mat[3] /= sum[1];mat[4] /= sum[1];mat[5] /= sum[1];
    mat[6] /= sum[2];mat[7] /= sum[2];mat[8] /= sum[2];
  }
};

typedef Matrix3x3<float> Matrix3x3f;

Matrix3x3f get_matrix_cam2rec709(const Matrix3x3f& xyzD65tocam)
{
    Matrix3x3f rgb2cam;
    // RGB2CAM =  XYZTOCAMRGB(colormatrix) * REC709TOXYZ
    rgb2cam = xyzD65tocam * Matrix3x3f(rec709toxyzD65);
    rgb2cam.normalize_rows();
    // Invert RGB2CAM matrix
    rgb2cam.invert_in_place();
    return rgb2cam;
}