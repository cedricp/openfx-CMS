#ifndef _video_object_
#define _video_object_

/* For MLV headers */
#include "pthread.h"
#include "llrawproc/llrawproc_object.h"
#include "mlv.h"


/* cache states */
#define MLV_FRAME_NOT_CACHED 0
#define MLV_FRAME_IS_CACHED 1
#define MLV_FRAME_BEING_CACHED 2

/* Struct of index of video and audio frames for quick access */
typedef struct
{
    uint16_t frame_type;     /* VIDF = 1, AUDF = 2, VERS = 3 */
    uint16_t chunk_num;      /* MLV chunk number */
    uint32_t frame_number;   /* Unique frame number */
    uint32_t frame_size;     /* Size of frame data */
    uint64_t frame_offset;   /* Offset to the start of frame data */
    uint64_t frame_time;     /* Time of frame from the start of recording in microseconds */
    uint64_t block_offset;   /* Offset to the start of the block header */
} frame_index_t;

/* MLV App map file header (.MAPP) */
#define MAPP_VERSION 3
typedef struct {
    uint8_t     fileMagic[4];  /* MAPP */
    uint64_t    mapp_size;     /* total MAPP file size */
    uint8_t     mapp_version;  /* MAPP structure version */
    uint32_t    block_num;     /* total block count */
    uint32_t    video_frames;  /* total video frames */
    uint32_t    audio_frames;  /* total audio frames */
    uint32_t    vers_blocks;   /* total VERS blocks */
    uint64_t    audio_size;    /* total size of audio data in bytes */
    uint64_t    df_offset;     /* offset to the dark frame location */
} mapp_header_t;

/* Struct for MLV handling */
typedef struct {

    /* Amount of MLV chunks (.MLV, .M00, .M01, ...) */
    int filenum;
    uint64_t block_num; /* How many file blocks in MLV file */

    /* 0=no, 1=yes, mlv file open */
    int is_active;

    /* MLV/Lite file(s) */
    FILE ** file;
    char * path;
    pthread_mutex_t * main_file_mutex; /* One for each file */
    pthread_mutex_t g_mutexFind; /* 'g' mutexes should prevent pink frames */
    pthread_mutex_t g_mutexCount;

    /* For access to MLV headers */
    mlv_file_hdr_t    MLVI;
    mlv_rawi_hdr_t    RAWI;
    mlv_rawc_hdr_t    RAWC;
    mlv_idnt_hdr_t    IDNT;
    mlv_expo_hdr_t    EXPO;
    mlv_lens_hdr_t    LENS;
    mlv_elns_hdr_t    ELNS;
    mlv_rtci_hdr_t    RTCI;
    mlv_wbal_hdr_t    WBAL;
    mlv_wavi_hdr_t    WAVI;
    mlv_diso_hdr_t    DISO;
    mlv_info_hdr_t    INFO;
    mlv_styl_hdr_t    STYL;
    mlv_vers_hdr_t    VERS;
    mlv_dark_hdr_t    DARK;
    mlv_vidf_hdr_t    VIDF; /* One of many VIDFs(don't know if they're different) */
    mlv_audf_hdr_t    AUDF; /* Last AUDF header read */

    char INFO_STRING[256]; /* String stored in INFO block */

    /* Dark frame info */
    uint64_t dark_frame_offset;

    /* Black and white level copy for resseting to the original */
    uint16_t original_black_level;
    uint16_t original_white_level;

    /* Video info */
    double      real_frame_rate; /* ...Because framerate is not explicitly stored in the file */
    double      frame_rate;      /* User may want to override it */
    uint32_t    frames;          /* Number of frames */
    uint32_t    frame_size;      /* NOT counting compression factor */
    frame_index_t * video_index;

    /* Audio info */
    uint32_t    audios;          /* Number of audio blocks */
    frame_index_t * audio_index;

    /* Audio buffer pointer and size */
    uint8_t * audio_data;        /* Audio buffer pointer */
    uint64_t  audio_size;        /* Aligned usable audio size */
    uint64_t  audio_buffer_size; /* Full audio buffer size to be freed */

    /* Version info */
    uint32_t    vers_blocks;     /* Number of audio blocks */
    frame_index_t * vers_index;

    /* Image processing object pointer (it is to be made separately) */
    llrawprocObject_t * llrawproc;

    /* Restricted lossless raw data bit depth */
    int lossless_bpp;

    /************************************************************
     *** CACHE AREA - used by getMlvProcessedFrame and things ***
     ************************************************************/

    /* 0 = no, 1 = (yes... cache threads are alive right now) */
    int is_caching;
    int cache_thread_count; /* Total active cache threads */
    uint64_t cache_next; /* Like a cache request (any non-zero frame) */
    pthread_mutex_t cache_mutex;
    /* Will be set to 1 for cache threads to stop (probably only by freeMlvObject) */
    int stop_caching;

    /* Decides whether or not AMaZE *has* to be used or not, normally disabled for smooth playback */
    int use_amaze;

    /* CA correction */
    //uint8_t ca_auto; /* off=0, on=1 */
    float ca_red;    /* Range -5..5 */
    float ca_blue;   /* Range -5..5 */

    /* How many cores, will not neccesarily determine number of threads made in any case, but helps */
    int cpu_cores; /* Default 4 */


} mlvObject_t;

#endif
