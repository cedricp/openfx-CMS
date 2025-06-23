
constant sampler_t samplerf =  CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

float3 gamma_correct(float3 color, float gamma)
{
    return pow(color, (float3)(1.0/gamma));
}

float3 inv_gamma_correct(float3 color, float gamma)
{
    return pow(color, (float3)(gamma));
}

float3 srgb_to_linear(float3 color)
{
    return (color <= (float3)(0.04045)) ? (color / (float3)(12.92)) : pow((color + (float3)(0.055)) / (float3)(1.055), (float3)(2.4));
}

float3 linear_to_srgb(float3 color)
{
    return (color <= (float3)(0.0031308)) ? (color * (float3)(12.92)) : pow(color, (float3)(1.0/2.4)) * (float3)(1.055) - (float3)(0.055);
}

kernel void
matrix_xform (read_only image2d_t in, write_only image2d_t out, constant float *colormatrix, int xstart, int ystart, int width, int height,
              int trc, int inverse)
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

    if (!inverse)
    {
        switch(trc){
            case 0: // Linear
                break;
            case 1: // Gamma 2.2
                pixel.xyz = inv_gamma_correct(pixel.xyz, 2.2);
                break;
            case 2: // Gamma 2.4
                pixel.xyz = inv_gamma_correct(pixel.xyz, 2.4);
                break;
            case 3: // Gamma 2.6
                pixel.xyz = inv_gamma_correct(pixel.xyz, 2.6);
                break;
            case 4: // sRGB
                pixel.xyz = srgb_to_linear(pixel.xyz);
                break;
        }
    }

    finalColor.x = colormatrix[0]*pixel.x + colormatrix[1]*pixel.y + colormatrix[2]*pixel.z;
    finalColor.y = colormatrix[3]*pixel.x + colormatrix[4]*pixel.y + colormatrix[5]*pixel.z;
    finalColor.z = colormatrix[6]*pixel.x + colormatrix[7]*pixel.y + colormatrix[8]*pixel.z;
    finalColor.w = pixel.w;

    if (inverse){
        switch(trc){
            case 0: // Linear
                break;
            case 1: // Gamma 2.2
                finalColor.xyz = gamma_correct(finalColor.xyz, 2.2);
                break;
            case 2: // Gamma 2.4
                finalColor.xyz = gamma_correct(finalColor.xyz, 2.4);
                break;
            case 3: // Gamma 2.6
                finalColor.xyz = gamma_correct(finalColor.xyz, 2.6);
                break;
            case 4: // sRGB
                finalColor.xyz = linear_to_srgb(finalColor.xyz);
                break;
        }
    }

    write_imagef (out, xy, finalColor);
}