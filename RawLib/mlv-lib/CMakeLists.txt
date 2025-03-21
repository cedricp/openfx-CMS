FILE(GLOB MLV_HEADERS *.h)
FILE(GLOB MLV_SOURCES *.c)
FILE(GLOB LJ92_SOURCES liblj92/lj92.c)
FILE(GLOB CAMID_SOURCES camid/camera_id.c)
FILE(GLOB DNG_SOURCES dng/dng.c)
FILE(GLOB LLRAW_SOURCES llrawproc/llrawproc.c llrawproc/darkframe.c llrawproc/dualiso.c llrawproc/hist.c llrawproc/patternnoise.c llrawproc/pixelproc.c llrawproc/stripes.c)

# Disable OpenMP - Use Natron's thread management instead
ADD_COMPILE_OPTIONS(-pthread -w) # -fopenmp)
ADD_LIBRARY(mlvlib_static OBJECT ${MLV_SOURCES} ${MLV_HEADERS} ${LJ92_SOURCES} ${CAMID_SOURCES} ${DNG_SOURCES} ${LLRAW_SOURCES})
INCLUDE_DIRECTORIES(${EIGEN3_INCLUDE_DIR} ${LibRaw_INCLUDE_DIR})
TARGET_INCLUDE_DIRECTORIES(mlvlib_static PUBLIC ./)