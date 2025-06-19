
constant sampler_t samplerf =  CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

kernel void
matrix_xform (read_only image2d_t in, write_only image2d_t out, constant float *colormatrix, int xstart, int ystart, int width, int height)
{
    const int2 xy = (int2)(get_global_id(0), get_global_id(1));

    if (xy.x < xstart || xy.y < ystart || xy.x >= xstart + width || xy.y >= ystart + height){
        float4 pix = (float4)(0.2,0.2,0.2,1.0);
        write_imagef (out, xy, pix);
        return;
    }

    const int2 xy_in = xy - (int2)(xstart, ystart);

    float4 pixel = read_imagef(in, samplerf, xy_in);
    float4 finalColor;

    finalColor.x = colormatrix[0]*pixel.x + colormatrix[1]*pixel.y + colormatrix[2]*pixel.z;
    finalColor.y = colormatrix[3]*pixel.x + colormatrix[4]*pixel.y + colormatrix[5]*pixel.z;
    finalColor.z = colormatrix[6]*pixel.x + colormatrix[7]*pixel.y + colormatrix[8]*pixel.z;

    finalColor.w = pixel.w;

    write_imagef (out, xy, finalColor);
}