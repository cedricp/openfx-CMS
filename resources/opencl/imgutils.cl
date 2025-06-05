
constant sampler_t samplerf =  CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

kernel void
matrix_xform (read_only image2d_t in, write_only image2d_t out, constant float *colormatrix)
{
    const int2 xy = (int2)(get_global_id(0), get_global_id(1));

    float4 pixel = read_imagef(in, samplerf, xy);
    float4 tmp;

    tmp.x = colormatrix[0]*pixel.x + colormatrix[1]*pixel.y + colormatrix[2]*pixel.z;
    tmp.y = colormatrix[3]*pixel.x + colormatrix[4]*pixel.y + colormatrix[5]*pixel.z;
    tmp.z = colormatrix[6]*pixel.x + colormatrix[7]*pixel.y + colormatrix[8]*pixel.z;

    tmp.w = pixel.w;

    write_imagef (out, xy, tmp);
}