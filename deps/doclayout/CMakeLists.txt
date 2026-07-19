cmake_minimum_required(VERSION 3.15)
project(DocLayoutV2Cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_definitions(-DNOMINMAX)
endif()

# 路径（由主项目传入）
if(NOT OpenCV_INCLUDE_DIRS)
    set(OpenCV_INCLUDE_DIRS "D:/project/cpp/opencv/build/include")
endif()
if(NOT OpenCV_LIB_DIR)
    set(OpenCV_LIB_DIR "D:/project/cpp/opencv/build/x64/vc16/lib")
endif()
if(NOT ONNXRUNTIME_ROOT)
    set(ONNXRUNTIME_ROOT "D:/project/cpp/onnxruntime-win-x64-gpu-1.20.1")
endif()

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${ONNXRUNTIME_ROOT}/include)
include_directories(include)

add_library(doclayout SHARED
        src/PPDocLayoutV2.cpp
        src/DocLayoutDLL.cpp
)

target_compile_definitions(doclayout PRIVATE DOCLAYOUT_EXPORTS)

target_link_libraries(doclayout
        ${OpenCV_LIB_DIR}/opencv_world480.lib
        ${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib
)
