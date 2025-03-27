/*
    This file is part of darktable,
    copyright (c) 2009--2010 johannes hanika.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#define NORM_MIN 1.52587890625e-05f // norm can't be < to 2^(-16)


constant sampler_t sampleri =  CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

constant sampler_t samplerf =  CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_LINEAR;

constant sampler_t samplerc =  CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP         | CLK_FILTER_NEAREST;

// sampler for when the bound checks are already done manually
constant sampler_t samplerA = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE            | CLK_FILTER_NEAREST;

static inline int
FC(const int row, const int col, const unsigned int filters)
{
  return filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3;
}

// static inline void matmut(float mat[9], float vec[3], float result[3])
// {
//     for (int i = 0; i < 3; i++)
//     {
//         float res = 0.0f;
//         for (int j = 0; j < 3; j++)
//         {
//             res += mat[i*3+j] * vec[j];
//         }
//         result[i] = res;
//     }
// }

kernel void
test_pattern (write_only image2d_t out)
{
  const int x = get_global_id(0);
  const int y = get_global_id(1);
  float4 color;

  float w = get_image_width(out);
  float h = get_image_height(out);

  color.x = x/w;
  color.y = y/h;

  write_imagef (out, (int2)(x, y), color);
}

/**
 * fill greens pass of pattern pixel grouping.
 * in (float) or (float4).x -> out (float4)
 */
kernel void
ppg_demosaic_green (read_only image2d_t in, write_only image2d_t out, const int width, const int height,
                    const unsigned int filters, const unsigned int black_level, const unsigned int white_level,
                    local float *buffer)
{
  const int x = get_global_id(0);
  const int y = get_global_id(1);
  const int xlsz = get_local_size(0);
  const int ylsz = get_local_size(1);
  const int xlid = get_local_id(0);
  const int ylid = get_local_id(1);
  const int xgid = get_group_id(0);
  const int ygid = get_group_id(1);

  // individual control variable in this work group and the work group size
  const int l = mad24(ylid, xlsz, xlid);
  const int lsz = mul24(xlsz, ylsz);

  // stride and maximum capacity of local buffer
  // cells of 1*float per pixel with a surrounding border of 3 cells
  const int stride = xlsz + 2*3;
  const int maxbuf = mul24(stride, ylsz + 2*3);

  // coordinates of top left pixel of buffer
  // this is 3 pixel left and above of the work group origin
  const int xul = mul24(xgid, xlsz) - 3;
  const int yul = mul24(ygid, ylsz) - 3;

  // populate local memory buffer
  for(int n = 0; n <= maxbuf/lsz; n++)
  {
    const int bufidx = mad24(n, lsz, l);
    if(bufidx >= maxbuf) continue;
    const int xx = xul + bufidx % stride;
    const int yy = yul + bufidx / stride;
    unsigned int val = read_imageui(in, sampleri, (int2)(xx, yy)).x;
    if(val < black_level) val = black_level;
    buffer[bufidx] = (float)(val - black_level) / (float)(white_level - black_level);
  }

  // center buffer around current x,y-Pixel
  buffer += mad24(ylid + 3, stride, xlid + 3);

  barrier(CLK_LOCAL_MEM_FENCE);

  // make sure we dont write the outermost 3 pixels
  if(x >= width - 3 || x < 3 || y >= height - 3 || y < 3) return;
  // process all non-green pixels
  const int row = y;
  const int col = x;
  const int c = FC(row, col, filters);
  float4 color; // output color

  const float pc = buffer[0];

  if     (c == 0) color.x = pc; // red
  else if(c == 1) color.y = pc; // green1
  else if(c == 2) color.z = pc; // blue
  else            color.y = pc; // green2

  // fill green layer for red and blue pixels:
  if(c == 0 || c == 2)
  {
    // look up horizontal and vertical neighbours, sharpened weight:
    const float pym  = buffer[-1 * stride];
    const float pym2 = buffer[-2 * stride];
    const float pym3 = buffer[-3 * stride];
    const float pyM  = buffer[ 1 * stride];
    const float pyM2 = buffer[ 2 * stride];
    const float pyM3 = buffer[ 3 * stride];
    const float pxm  = buffer[-1];
    const float pxm2 = buffer[-2];
    const float pxm3 = buffer[-3];
    const float pxM  = buffer[ 1];
    const float pxM2 = buffer[ 2];
    const float pxM3 = buffer[ 3];
    const float guessx = (pxm + pc + pxM) * 2.0f - pxM2 - pxm2;
    const float diffx  = (fabs(pxm2 - pc) +
                          fabs(pxM2 - pc) +
                          fabs(pxm  - pxM)) * 3.0f +
                         (fabs(pxM3 - pxM) + fabs(pxm3 - pxm)) * 2.0f;
    const float guessy = (pym + pc + pyM) * 2.0f - pyM2 - pym2;
    const float diffy  = (fabs(pym2 - pc) +
                          fabs(pyM2 - pc) +
                          fabs(pym  - pyM)) * 3.0f +
                         (fabs(pyM3 - pyM) + fabs(pym3 - pym)) * 2.0f;
    if(diffx > diffy)
    {
      // use guessy
      const float m = fmin(pym, pyM);
      const float M = fmax(pym, pyM);
      color.y = fmax(fmin(guessy*0.25f, M), m);
    }
    else
    {
      const float m = fmin(pxm, pxM);
      const float M = fmax(pxm, pxM);
      color.y = fmax(fmin(guessx*0.25f, M), m);
    }
  }
  write_imagef (out, (int2)(x,y), fmax(color, 0.0f));
}


/**
 * fills the reds and blues in the gaps (done after ppg_demosaic_green).
 * in (float4) -> out (float4)
 */
kernel void
ppg_demosaic_redblue (read_only image2d_t in, write_only image2d_t out, const int width, const int height,
                      const unsigned int filters, constant float *cameraMatrix, local float4 *buffer)
{
  // image in contains full green and sparse r b
  const int x = get_global_id(0);
  const int y = get_global_id(1);
  const int xlsz = get_local_size(0);
  const int ylsz = get_local_size(1);
  const int xlid = get_local_id(0);
  const int ylid = get_local_id(1);
  const int xgid = get_group_id(0);
  const int ygid = get_group_id(1);

  // individual control variable in this work group and the work group size
  const int l = mad24(ylid, xlsz, xlid);
  const int lsz = mul24(xlsz, ylsz);

  // stride and maximum capacity of local buffer
  // cells of float4 per pixel with a surrounding border of 1 cell
  const int stride = xlsz + 2;
  const int maxbuf = mul24(stride, ylsz + 2);

  // coordinates of top left pixel of buffer
  // this is 1 pixel left and above of the work group origin
  const int xul = mul24(xgid, xlsz) - 1;
  const int yul = mul24(ygid, ylsz) - 1;

  // populate local memory buffer
  for(int n = 0; n <= maxbuf/lsz; n++)
  {
    const int bufidx = mad24(n, lsz, l);
    if(bufidx >= maxbuf) continue;
    const int xx = xul + bufidx % stride;
    const int yy = yul + bufidx / stride;
    buffer[bufidx] = read_imagef(in, sampleri, (int2)(xx, yy));
  }

  // center buffer around current x,y-Pixel
  buffer += mad24(ylid + 1, stride, xlid + 1);

  barrier(CLK_LOCAL_MEM_FENCE);

  if(x >= width || y >= height) return;
  const int row = y;
  const int col = x;
  const int c = FC(row, col, filters);
  float4 color = buffer[0];
  if(x == 0 || y == 0 || x == (width-1) || y == (height-1))
  {
    write_imagef (out, (int2)(x, y), fmax(color, 0.0f));  
    return;
  }

  if(c == 1 || c == 3)
  { // calculate red and blue for green pixels:
    // need 4-nbhood:
    float4 nt = buffer[-stride];
    float4 nb = buffer[ stride];
    float4 nl = buffer[-1];
    float4 nr = buffer[ 1];
    if(FC(row, col+1, filters) == 0) // red nb in same row
    {
      color.z = (nt.z + nb.z + 2.0f*color.y - nt.y - nb.y)*0.5f;
      color.x = (nl.x + nr.x + 2.0f*color.y - nl.y - nr.y)*0.5f;
    }
    else
    { // blue nb
      color.x = (nt.x + nb.x + 2.0f*color.y - nt.y - nb.y)*0.5f;
      color.z = (nl.z + nr.z + 2.0f*color.y - nl.y - nr.y)*0.5f;
    }
  }
  else
  {
    // get 4-star-nbhood:
    float4 ntl = buffer[-stride - 1];
    float4 ntr = buffer[-stride + 1];
    float4 nbl = buffer[ stride - 1];
    float4 nbr = buffer[ stride + 1];

    if(c == 0)
    { // red pixel, fill blue:
      const float diff1  = fabs(ntl.z - nbr.z) + fabs(ntl.y - color.y) + fabs(nbr.y - color.y);
      const float guess1 = ntl.z + nbr.z + 2.0f*color.y - ntl.y - nbr.y;
      const float diff2  = fabs(ntr.z - nbl.z) + fabs(ntr.y - color.y) + fabs(nbl.y - color.y);
      const float guess2 = ntr.z + nbl.z + 2.0f*color.y - ntr.y - nbl.y;
      if     (diff1 > diff2) color.z = guess2 * 0.5f;
      else if(diff1 < diff2) color.z = guess1 * 0.5f;
      else color.z = (guess1 + guess2)*0.25f;
    }
    else // c == 2, blue pixel, fill red:
    {
      const float diff1  = fabs(ntl.x - nbr.x) + fabs(ntl.y - color.y) + fabs(nbr.y - color.y);
      const float guess1 = ntl.x + nbr.x + 2.0f*color.y - ntl.y - nbr.y;
      const float diff2  = fabs(ntr.x - nbl.x) + fabs(ntr.y - color.y) + fabs(nbl.y - color.y);
      const float guess2 = ntr.x + nbl.x + 2.0f*color.y - ntr.y - nbl.y;
      if     (diff1 > diff2) color.x = guess2 * 0.5f;
      else if(diff1 < diff2) color.x = guess1 * 0.5f;
      else color.x = (guess1 + guess2)*0.25f;
    }
  }
  float4 tmp;

  color.x *= cameraMatrix[9];
  color.y *= cameraMatrix[10];
  color.z *= cameraMatrix[11];

  tmp.x = cameraMatrix[0]*color.x + cameraMatrix[1]*color.y + cameraMatrix[2]*color.z;
  tmp.y = cameraMatrix[3]*color.x + cameraMatrix[4]*color.y + cameraMatrix[5]*color.z;
  tmp.z = cameraMatrix[6]*color.x + cameraMatrix[7]*color.y + cameraMatrix[8]*color.z;

  

  color.x = cameraMatrix[12]*tmp.x + cameraMatrix[13]*tmp.y + cameraMatrix[14]*tmp.z;
  color.y = cameraMatrix[15]*tmp.x + cameraMatrix[16]*tmp.y + cameraMatrix[17]*tmp.z;
  color.z = cameraMatrix[18]*tmp.x + cameraMatrix[19]*tmp.y + cameraMatrix[20]*tmp.z;

  color.w = 1.f;

  write_imagef (out, (int2)(x,  height - 1 - y), fmax(color, 0.0f));
}

/**
 * Demosaic image border
 */
kernel void
border_interpolate(read_only image2d_t in, write_only image2d_t out,
                   const int width, const int height, const unsigned int filters,
                   const int border, const unsigned int black_level, const unsigned int white_level)
{
  const int x = get_global_id(0);
  const int y = get_global_id(1);

  if(x >= width || y >= height) return;

  const int avgwindow = 1;

  if(x >= border && x < width-border && y >= border && y < height-border) return;

  float4 o;
  float sum[4] = { 0.0f };
  int count[4] = { 0 };

  for (int j=y-avgwindow; j<=y+avgwindow; j++) for (int i=x-avgwindow; i<=x+avgwindow; i++)
  {
    if (j>=0 && i>=0 && j<height && i<width)
    {
      const int f = FC(j,i,filters);
      unsigned short val = read_imageui(in, sampleri, (int2)(i, j)).x;
      if(val < black_level) val = black_level;
      sum[f] += (float)(val - black_level) / (float)(white_level - black_level);
      count[f]++;
    }
  }

  //const float i = read_imagef(in, sampleri, (int2)(x, y)).x;
  
  unsigned int val = read_imageui(in, sampleri, (int2)(x, y)).x;
  if(val < black_level) val = black_level;
  const float i = (float)(val - black_level) / (float)(white_level - black_level);

  o.x = count[0] > 0 ? sum[0]/count[0] : i;
  o.y = count[1]+count[3] > 0 ? (sum[1]+sum[3])/(count[1]+count[3]) : i;
  o.z = count[2] > 0 ? sum[2]/count[2] : i;

  const int f = FC(y,x,filters);

  if     (f == 0) o.x = i;
  else if(f == 1) o.y = i;
  else if(f == 2) o.z = i;
  else            o.y = i;

  write_imagef (out, (int2)(x, y), fmax(o, 0.0f));
}