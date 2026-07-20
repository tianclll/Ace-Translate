cmake_minimum_required(VERSION 3.10)
project(TranslationApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# llama.cpp 路径（已编译好的库文件）
set(LLAMA_DIR "D:/AceTranslatePro/tools/translator/TranslationApp/external/llama.cpp")
set(LLAMA_BUILD_DIR "${LLAMA_DIR}/build")

# 包含路径
include_directories(${LLAMA_DIR})
include_directories(${LLAMA_DIR}/include)
include_directories(${LLAMA_DIR}/ggml/include)
include_directories(${LLAMA_DIR}/common)

# 创建动态库
add_library(translator SHARED src/translator.cpp)

target_compile_definitions(translator PRIVATE TRANSLATOR_EXPORTS)
# CPU 版：不定义 GGML_USE_CUDA

# 链接已编译好的 llama 库
target_link_libraries(translator PRIVATE
        ${LLAMA_BUILD_DIR}/src/Release/llama.lib
        ${LLAMA_BUILD_DIR}/common/Release/llama-common.lib
        ${CMAKE_THREAD_LIBS_INIT}
)

if(WIN32)
    target_link_libraries(translator PRIVATE ws2_32 kernel32 user32 shell32 advapi32)
    target_compile_definitions(translator PRIVATE _CRT_SECURE_NO_WARNINGS _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
endif()

target_compile_features(translator PRIVATE cxx_std_17)
