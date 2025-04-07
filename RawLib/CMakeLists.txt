ADD_SUBDIRECTORY(mlv-lib)

FILE(GLOB MLV_SOURCES_CPP *.cpp)
FILE(GLOB LENSID_SOURCES lensid/lens_id.cpp)
FILE(GLOB IDT_DNG_SOURCES idt/dng_idt.cpp idt/spectral_idt.cpp)

ADD_COMPILE_OPTIONS(-pthread -w)

ADD_LIBRARY(rawlib_static OBJECT ${LENSID_SOURCES} ${MLV_SOURCES_CPP} ${IDT_SOURCES} ${IDT_DNG_SOURCES})
INCLUDE_DIRECTORIES(${EIGEN3_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../libs/libRaw/libraw ${CMAKE_CURRENT_SOURCE_DIR}/mlv-lib/dng)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../libs/ceres/include ${CMAKE_CURRENT_SOURCE_DIR}/../libs/glog/src ${CMAKE_CURRENT_SOURCE_DIR}/idt)
target_link_libraries(rawlib_static PUBLIC glog)
TARGET_INCLUDE_DIRECTORIES(rawlib_static PUBLIC . ../utils mlv-lib lensid)