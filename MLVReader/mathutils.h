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
    0.0193339,  0.1191920,  0.9503041,
};

inline void matrix_vector_mult(const float *mat, const float *vec, float *result)
{
    for (int i = 0; i < 3; i++)
    {
        float res = 0.0;
        for (int j = 0; j < 3; j++)
        {
            res += mat[i*3+j] * vec[j];
        }
        result[i] = res;
    }
}

inline void mat_mat_mult(const float *mat1, const float *mat2, float *result)
{
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            float res = 0.0;
            for (int k = 0; k < 3; k++)
            {
                res += mat1[i*3+k] * mat2[k*3+j];
            }
            result[i*3+j] = res;
        }
    }
}

inline void invert_matrix(const float *mat, float *result)
{
    float det = mat[0] * (mat[4] * mat[8] - mat[5] * mat[7]) -
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

inline void get_matrix_cam2rec709(float colormatrix[9], float result[9])
{
    float rgb2cam[9];
    // RGB2CAM =  XYZTOCAMRGB(colormatrix) * REC709TOXYZ
    mat_mat_mult(colormatrix, rec709toxyzD65, rgb2cam);
    float sum[3] = {rgb2cam[0] + rgb2cam[1] + rgb2cam[2],
        rgb2cam[3] + rgb2cam[4] + rgb2cam[5],
        rgb2cam[6] + rgb2cam[7] + rgb2cam[8]};
    // Normalize rows
    rgb2cam[0] /= sum[0];rgb2cam[1] /= sum[0];rgb2cam[2] /= sum[0];
    rgb2cam[3] /= sum[1];rgb2cam[4] /= sum[1];rgb2cam[5] /= sum[1];
    rgb2cam[6] /= sum[2];rgb2cam[7] /= sum[2];rgb2cam[8] /= sum[2];
    // Invert RGB2CAM matrix
    invert_matrix(rgb2cam, result);
}