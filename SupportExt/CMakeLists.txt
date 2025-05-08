FILE(GLOB SUPPORTEXT_SOURCES
    "tinythread.cpp"
    "ofxsThreadSuite.cpp"
    "ofxsLut.cpp"
    "ofxsGenerator.cpp"
    "ofxsLut.cpp"
    "ofxsMultiPlane.cpp"
    "ofxsOGLTextRenderer.cpp"
    "ofxsOGLFontData.cpp"
    "ofxsRamp.cpp"
    "ofxsShutter.cpp"
    "ofxsTracking.cpp"
    "ofxsTransform3x3.cpp"
    "ofxsTransformInteract.cpp"
    "ofxsRectangleInteract.cpp"
    "*.h"
)

INCLUDE_DIRECTORIES(${OPENFX_PATH}/include)
INCLUDE_DIRECTORIES(${OPENFX_PATH}/Support/include)
INCLUDE_DIRECTORIES(${OPENFX_PATH}/Support/Plugins/include)

ADD_LIBRARY(supportext-static STATIC ${SUPPORTEXT_SOURCES})
TARGET_COMPILE_DEFINITIONS(supportext-static PRIVATE ${TARGET_DEFS})