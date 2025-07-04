#include <string.h>
#include <math.h>
#include "ColorAberrationCorrection.h"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

static void rmCA(float* rVec, float* gVec, float* bVec, int width, int height, float threshold, int radius)
{
    for (int y = 0; y < height; ++y)
	{
        float *bptr = &bVec[y*width];
        float *gptr = &gVec[y*width];
        float *rptr = &rVec[y*width];

        for (int x = 2; x < width - 2; ++x)
		{
			//find the edge by finding green channel gradient bigger than threshold
            float diff = gptr[x + 1] - gptr[x - 1];
			if (fabsf(diff) >= threshold)
			{
				// +/- sign of this edge
				float sign = diff > 0 ? 1 : -1;

				//Searching the boundary for correction range
				int lpos = x-1, rpos = x+1;
                for (; lpos > 1; --lpos)
				{
					//make sure the gradient is the same sign with edge
					float ggrad = (gptr[lpos + 1] - gptr[lpos - 1])*sign;
					float bgrad = (bptr[lpos + 1] - bptr[lpos - 1])*sign;
					float rgrad = (rptr[lpos + 1] - rptr[lpos - 1])*sign;
                    if ( x-lpos >= radius ) { break; }
                    if (MAX(MAX(bgrad, ggrad), rgrad) < threshold) { break; }
                }
				lpos -= 1;
                for (; rpos < width - 2; ++rpos)
				{
					//make sure the gradient is the same sign with edge
					float ggrad = (gptr[rpos + 1] - gptr[rpos - 1])*sign;
					float bgrad = (bptr[rpos + 1] - bptr[rpos - 1])*sign;
					float rgrad = (rptr[rpos + 1] - rptr[rpos - 1])*sign;
                    if ( rpos-x >= radius ) { break; }
                    if (MAX(MAX(bgrad, ggrad), rgrad) < threshold) { break; }
                }
				rpos += 1;

				//record the maximum and minimum color difference between R&G and B&G of range boundary
                float bgmaxVal = MAX(bptr[lpos] - gptr[lpos], bptr[rpos] - gptr[rpos]);
                float bgminVal = MIN(bptr[lpos] - gptr[lpos], bptr[rpos] - gptr[rpos]);
                float rgmaxVal = MAX(rptr[lpos] - gptr[lpos], rptr[rpos] - gptr[rpos]);
                float rgminVal = MIN(rptr[lpos] - gptr[lpos], rptr[rpos] - gptr[rpos]);

				for (int k = lpos; k <= rpos; ++k)
				{
					float bdiff = bptr[k] - gptr[k];
					float rdiff = rptr[k] - gptr[k];

					//Replace the B or R value if its color difference of R/G and B/G is bigger(smaller)
					//than maximum(minimum) of color difference on range boundary
                    bptr[k] =  bdiff > bgmaxVal ? bgmaxVal + gptr[k] :
						(bdiff < bgminVal ? bgminVal + gptr[k] : bptr[k]);
                    rptr[k] =  rdiff > rgmaxVal ? rgmaxVal + gptr[k] :
						(rdiff < rgminVal ? rgminVal + gptr[k] : rptr[k]) ;			
				}
				x = rpos - 2;
			}
		}
	}
}

/* Filter CAs and ColorMoiree in RGB picture data */
void CACorrection(int imageX, int imageY,
                  float * __restrict inputImage,
                  float threshold, uint8_t radius)
{
    //getting working memory
    float *bVec = malloc( imageX * imageY * sizeof( float ) );
    float *gVec = malloc( imageX * imageY * sizeof( float ) );
    float *rVec = malloc( imageX * imageY * sizeof( float ) );
    float *temp = malloc( imageX * imageY * sizeof( float ) );

	//split the color image into individual color channel for convenient in calculation
    for( int i = 0, j = 0; i < imageX*imageY*4; i+=4, j++ )
    {
        rVec[j] = inputImage[i+0];
        gVec[j] = inputImage[i+1];
        bVec[j] = inputImage[i+2];
    }

    //first run
    rmCA(rVec, gVec, bVec, imageX, imageY, threshold, radius);

	//transpose the R,G B channel image to correct chromatic aberration in vertical direction 
    memcpy( temp, rVec, imageX*imageY * sizeof(float) );
    for( int y = 0; y < imageY; y++ )
        for( int x = 0; x < imageX; x++ )
            rVec[x*imageY+y] = temp[y*imageX+x];
    memcpy( temp, gVec, imageX*imageY * sizeof(float) );
    for( int y = 0; y < imageY; y++ )
        for( int x = 0; x < imageX; x++ )
            gVec[x*imageY+y] = temp[y*imageX+x];
    memcpy( temp, bVec, imageX*imageY * sizeof(float) );
    for( int y = 0; y < imageY; y++ )
        for( int x = 0; x < imageX; x++ )
            bVec[x*imageY+y] = temp[y*imageX+x];

    //second run
    rmCA(rVec, gVec, bVec, imageY, imageX, threshold, radius);

    //rotate the image back to original position
    memcpy( temp, rVec, imageX*imageY * sizeof(float) );
    for( int y = 0; y < imageY; y++ )
        for( int x = 0; x < imageX; x++ )
            rVec[y*imageX+x] = temp[x*imageY+y];
    memcpy( temp, gVec, imageX*imageY * sizeof(float) );
    for( int y = 0; y < imageY; y++ )
        for( int x = 0; x < imageX; x++ )
            gVec[y*imageX+x] = temp[x*imageY+y];
    memcpy( temp, bVec, imageX*imageY * sizeof(float) );
    for( int y = 0; y < imageY; y++ )
        for( int x = 0; x < imageX; x++ )
            bVec[y*imageX+x] = temp[x*imageY+y];

    //merge channels into final image
    for( int i = 0; i < imageX*imageY; i++ )
    {
        int j = i * 4;
        inputImage[j+0] = rVec[i];
        inputImage[j+1] = gVec[i];
        inputImage[j+2] = bVec[i];
        inputImage[j+3] = 1.0f; // Set alpha channel to 1.0 (opaque)
    }

    //clean up
    free( bVec );
    free( gVec );
    free( rVec );
    free( temp );
}
