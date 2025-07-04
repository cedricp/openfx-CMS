#ifndef COLORABBERATIONCORRECTION
#define COLORABBERATIONCORRECTION

#include <stdlib.h>
#include <stdint.h>

void CACorrection(int imageX, int imageY,
                  float * __restrict inputImage,
                  float threshold, uint8_t radius);

#endif
