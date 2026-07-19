cmake_minimum_required(VERSION 3.15)
project(VLMVisualCpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# llama.cpp 路径（CPU 版：使用 CPU 编译的库）
set(LLAMA_DIR "D:/AceTranslatePro/tools/translator/TranslationApp/external/llama.cpp")
set(LLAMA_BUILD_DIR "${LLAMA_DIR}/build")

include_directories(${LLAMA_DIR})
include_directories(${LLAMA_DIR}/include)
include_directories(${LLAMA_DIR}/ggml/include)
include_directories(${LLAMA_DIR}/ggml/src)
include_directories(${LLAMA_DIR}/common)
include_directories(${LLAMA_DIR}/vendor)
include_directories(${LLAMA_DIR}/tools/mtmd)
include_directories(${LLAMA_DIR}/tools/mtmd/models)
include_directories(${LLAMA_DIR}/tools/mtmd/helper)

add_library(vlm_visual SHARED src/vlm_visual.cpp)

target_compile_definitions(vlm_visual PRIVATE VLM_VISUAL_EXPORTS)
# CPU 版：不定义 GGML_USE_CUDA，会走 CPU 降级逻辑

target_include_directories(vlm_visual PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# CPU 版：不链接 ggml-cuda.lib
target_link_libraries(vlm_visual PRIVATE
        ${LLAMA_BUILD_DIR}/src/Release/llama.lib
        ${LLAMA_BUILD_DIR}/common/Release/llama-common.lib
        ${LLAMA_BUILD_DIR}/ggml/src/Release/ggml.lib
        ${LLAMA_BUILD_DIR}/ggml/src/Release/ggml-base.lib
        ${LLAMA_BUILD_DIR}/ggml/src/Release/ggml-cpu.lib
        ${LLAMA_BUILD_DIR}/tools/mtmd/Release/mtmd.lib
        ${CMAKE_THREAD_LIBS_INIT}
)

if(WIN32)
    target_link_libraries(vlm_visual PRIVATE ws2_32 kernel32 user32 shell32 advapi32)
    target_compile_definitions(vlm_visual PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
