FILE(GLOB LIBRAW_SRC
    src/libraw_c_api.cpp
    src/libraw_datastream.cpp
    src/decoders/*.cpp
    src/demosaic/*.cpp
    src/integration/*.cpp
    src/metadata/*.cpp
    src/postprocessing/*.cpp
    src/preprocessing/*.cpp
    src/tables/*.cpp
    src/utils/*.cpp
    src/write/*.cpp
    src/x3f/*.cpp
)

INCLUDE_DIRECTORIES(.)

ADD_LIBRARY(libraw_static STATIC ${LIBRAW_SRC})
SET_TARGET_PROPERTIES(libraw_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
