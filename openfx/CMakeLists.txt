set(OFX_SUPPORT_HEADER_DIR "Support/include")
set(OFX_SUPPORT_LIBRARY_DIR "Support/Library")

FILE(GLOB SUPPORT_SOURCES
    "${OFX_SUPPORT_HEADER_DIR}/*.h"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsCore.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsImageEffect.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsInteract.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsLog.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsMultiThread.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsParams.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsProperty.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsPropertyValidation.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/*.h"
)


INCLUDE_DIRECTORIES(include)
INCLUDE_DIRECTORIES(Support/include)
INCLUDE_DIRECTORIES(Support/Plugins/include)

ADD_LIBRARY(openfx-static STATIC ${SUPPORT_SOURCES})
TARGET_COMPILE_DEFINITIONS(openfx-static PRIVATE ${TARGET_DEFS})