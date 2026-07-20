cmake_minimum_required(VERSION 3.15)
project(ASR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CPU 版 ONNXRuntime
set(ONNXRUNTIME_ROOT "D:/project/cpp/onnxruntime-win-x64-gpu-1.20.1")

include_directories(${ONNXRUNTIME_ROOT}/include)
include_directories(include)
include_directories(D:/AceTranslatePro/AceTranslatePro/third_party)
include_directories(D:/project/cpp/opencv/build/include)

add_library(asr SHARED
        src/SenseVoiceDLL.cpp
        src/SenseVoiceEngine.cpp
        src/AudioCapture.cpp
        src/Fbank.cpp
        src/CMVN.cpp
)

target_compile_definitions(asr PRIVATE ASR_EXPORTS)

target_link_libraries(asr
        ${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib
        winmm.lib
        D:/project/cpp/opencv/build/x64/vc16/lib/opencv_world480.lib
)
