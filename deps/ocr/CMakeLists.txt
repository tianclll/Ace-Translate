cmake_minimum_required(VERSION 3.15)
project(ocr_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ========== Windows ==========
if(WIN32)
    add_definitions(-DNOMINMAX -D_WIN32_WINNT=0x0A00)
endif()

# ========== 路径（由主项目传入，独立编译时使用默认值）=====
if(NOT ONNXRUNTIME_ROOT)
    set(ONNXRUNTIME_ROOT "D:/project/cpp/onnxruntime-win-x64-gpu-1.20.1")
endif()
if(NOT OpenCV_LIB_DIR)
    set(OpenCV_LIB_DIR "D:/project/cpp/opencv/build/x64/vc16/lib")
endif()
if(NOT OpenCV_INCLUDE_DIRS)
    set(OpenCV_INCLUDE_DIRS "D:/project/cpp/opencv/build/include")
endif()

# ========== ONNXRuntime ==========
set(onnxruntime_INCLUDE_DIRS "${ONNXRUNTIME_ROOT}/include")
set(onnxruntime_LIBRARIES "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib")

# ========== Clipper2 ==========
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/Clipper2/include)
set(CLIPPER2_SOURCES
        third_party/Clipper2/src/clipper.engine.cpp
        third_party/Clipper2/src/clipper.offset.cpp
        third_party/Clipper2/src/clipper.rectclip.cpp
)

# ========== 包含目录 ==========
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${onnxruntime_INCLUDE_DIRS})
include_directories(${OpenCV_INCLUDE_DIRS})

# ========== 源文件 ==========
set(SOURCES
        src/ocr.cpp
        src/predict_det.cpp
        src/predict_rec.cpp
        src/predict_system.cpp
        src/db_postprocess.cpp
        src/utils.cpp
        src/predict_cls.cpp
        ${CLIPPER2_SOURCES}
)

set(HEADERS
        include/ocr.hpp
        include/predict_system.hpp
        include/predict_det.hpp
        include/predict_rec.hpp
        include/db_postprocess.hpp
        include/utils.hpp
        include/predict_cls.hpp
        include/types.hpp
        include/export.hpp
)

# ========== DLL ==========
add_library(ocr SHARED ${SOURCES} ${HEADERS})
target_compile_definitions(ocr PRIVATE OCR_EXPORTS)

if(WIN32)
    set_target_properties(ocr PROPERTIES OUTPUT_NAME "ocr" PREFIX "" SUFFIX ".dll")
endif()

target_link_libraries(ocr
        ${onnxruntime_LIBRARIES}
        ${OpenCV_LIB_DIR}/opencv_world480.lib
)
