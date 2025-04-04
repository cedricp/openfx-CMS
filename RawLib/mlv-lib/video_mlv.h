#ifndef _video_mlv_
#define _video_mlv_

#include "macros.h"
#include "mlv.h"
#include "mlv_object.h"
#include "raw.h"

/* All functions in one */
mlvObject_t * initMlvObjectWithClip(const char * mlvPath, int * err, char * error_message);
void initMlvFiles(const char* path, mlvObject_t * videodest);
/* Initialises an MLV object. That's all you need to know */
mlvObject_t * initMlvObject();

/* Prints everything you'll ever need to know */
void printMlvInfo(mlvObject_t * video);

/* Reads an MLV file in to a video object(mlvObject_t struct)
 * only puts frame indexes and metadata in to the mlvObject_t, 
 * no debayering or processing */
int openMlvClip(mlvObject_t * video, const char * mlvPath, char * error_message);
/* return error codes of and open modes of openMlvClip() */
enum mlv_err { MLV_ERR_NONE, MLV_ERR_OPEN, MLV_ERR_IO, MLV_ERR_CORRUPTED, MLV_ERR_INVALID };

/* Functions for saving cut or averaged MLV */
int saveMlvHeaders(mlvObject_t * video, FILE * output_mlv, int export_audio, int export_mode, uint32_t frame_start, uint32_t frame_end, const char * version, char * error_message);
int saveMlvAVFrame(mlvObject_t * video, FILE * output_mlv, int export_audio, int export_mode, uint32_t frame_start, uint32_t frame_end, uint32_t frame_index, uint64_t * avg_buf, char * error_message);
enum export_mode { MLV_FAST_PASS, MLV_COMPRESS, MLV_DECOMPRESS, MLV_AVERAGED_FRAME, MLV_DF_INT };
/* from darkframe.c */
extern int df_init(mlvObject_t * video);

/* Frees all memory and closes file */
void freeMlvObject(mlvObject_t * video, int shared);

/* Unpacks the bits of a frame to get a bayer B&W image (without black level correction)
 * Needs memory to return to, sized: sizeof(float) * getMlvHeight(urvid) * getMlvWidth(urvid)
 * Output values will be in range 0-65535 (16 bit), float is only because AMAzE uses it */
int getMlvRawFrameUint16(mlvObject_t * video, uint64_t frameIndex, uint16_t * unpackedFrame);
void getMlvRawFrameFloat(mlvObject_t * video, uint64_t frameIndex, float * outputFrame);

/* Gets image aspect ratio according to RAWC block info, calculating from binnin + skipping values */
float getMlvAspectRatio(mlvObject_t * video);

/* Set imaginary lossless bit depth value */
void setMlvLosslessBpp(mlvObject_t * video);

/******************************** 
 ********* PRIVATE AREA *********
 ********************************/

 FILE **load_all_chunks(const char *base_filename, int *entries);
 


#endif
