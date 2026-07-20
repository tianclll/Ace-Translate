cmake_minimum_required(VERSION 3.10)
project(AceTranslatePro)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHa")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ------------------ Qt ------------------
set(QT_DIR "D:/Qt/6.5.2/msvc2019_64")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${QT_DIR})
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ------------------ OpenCV ------------------
set(OpenCV_INCLUDE_DIRS "D:/project/cpp/opencv/build/include")
set(OpenCV_LIB_DIR "D:/project/cpp/opencv/build/x64/vc16/lib")
set(OpenCV_BIN_DIR "D:/project/cpp/opencv/build/x64/vc16/bin")

# ------------------ ONNXRuntime (GPU) ------------------
set(ONNXRUNTIME_ROOT "D:/project/cpp/onnxruntime-win-x64-gpu-1.20.1")

# ------------------ PDFium ------------------
set(PDFIUM_ROOT "${CMAKE_SOURCE_DIR}/third_party/pdfium")
set(PDFIUM_INCLUDE_DIR "${PDFIUM_ROOT}/include")
set(PDFIUM_LIB_DIR "${PDFIUM_ROOT}/lib")
set(PDFIUM_LIBRARY "${PDFIUM_LIB_DIR}/pdfium.dll.lib")

# ------------------ 头文件目录 ------------------
include_directories(include)
include_directories(ui)
include_directories(third_party)
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${PDFIUM_INCLUDE_DIR})

# ------------------ 源文件列表 ------------------
set(SOURCES
        ui/main.cpp
        ui/mainwindow.cpp
        ui/floatwindow.cpp
        ui/regioncapture.cpp
        ui/toast.cpp
        ui/zoomablelabel.cpp
        ui/splashscreen.cpp
        ui/resources.qrc
        ui/app_icon.rc

        src/engines/DLLLoader.cpp
        src/engines/LayoutDetector.cpp
        src/engines/OCREngine.cpp
        src/engines/VLMEngine.cpp
        src/engines/TranslatorEngine.cpp

        src/utils/ImageUtils.cpp
        src/utils/MarkdownGenerator.cpp
        src/utils/FormatFixer.cpp
        src/utils/OfficeConverter.cpp

        src/processors/DocumentProcessor.cpp

        src/core/ConfigManager.cpp
        src/core/GlobalEngineContext.cpp

        src/modules/FileTranslationModule.cpp
        src/modules/PhotoTranslationModule.cpp
        src/modules/TextTranslationModule.cpp
        src/modules/ScreenshotTranslationModule.cpp

        src/DocumentEngine.cpp
        src/DocumentTextEngine.cpp
        src/utils/FontManager.cpp
        src/utils/ImageLayout.cpp
        src/utils/ImageRenderer.cpp
)

message(STATUS "SOURCES: ${SOURCES}")

# ------------------ 可执行文件 ------------------
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# ------------------ 链接库 ------------------
target_link_libraries(${PROJECT_NAME}
    $<$<CONFIG:Debug>:${OpenCV_LIB_DIR}/opencv_world480d.lib>
    $<$<NOT:$<CONFIG:Debug>>:${OpenCV_LIB_DIR}/opencv_world480.lib>
)
target_link_libraries(${PROJECT_NAME} ${PDFIUM_LIBRARY})
target_link_libraries(${PROJECT_NAME} Qt6::Widgets)
target_link_libraries(${PROJECT_NAME} ole32 sapi winmm)

# ====== deps 子项目（GPU 版）======
add_subdirectory(deps/translator)
add_subdirectory(deps/vlm)
add_subdirectory(deps/ocr)
add_subdirectory(deps/doclayout)
add_subdirectory(deps/uvdoc)
add_subdirectory(deps/asr)

# ====== 自动复制 DLL ======
foreach(TGT ocr doclayout vlm_visual translator DocumentImageProcessor asr)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TGT}>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endforeach()

set(OUT_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

# VC++ 运行库（目标电脑可能缺失）
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/Windows/System32/VCRUNTIME140.dll" "${OUT_DIR}/"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/Windows/System32/VCRUNTIME140_1.dll" "${OUT_DIR}/"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/Windows/System32/libomp140.x86_64.dll" "${OUT_DIR}/"
)

# ONNXRuntime（GPU 版：复制核心 DLL + providers）
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll" "${OUT_DIR}/"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_cuda.dll" "${OUT_DIR}/"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_shared.dll" "${OUT_DIR}/"
)

# OpenCV
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${OpenCV_BIN_DIR}/opencv_world480.dll" "${OUT_DIR}/"
)

# PDFium
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PDFIUM_ROOT}/bin/pdfium.dll" "${OUT_DIR}/"
)

# llama 运行时 DLL（GPU 版）
set(LLAMA_BIN_DIR "D:/AceTranslatePro/tools/translator/TranslationApp/external/llama.cpp/build_gpu/bin/Release")
foreach(DLL llama.dll llama-common.dll ggml.dll ggml-base.dll ggml-cpu.dll ggml-cuda.dll mtmd.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_BIN_DIR}/${DLL}" "${OUT_DIR}/"
    )
endforeach()

# CUDA 运行时 DLL（GPU 版必需）
set(CUDA_BIN_DIR "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.1/bin")
foreach(DLL cudart64_12.dll cublas64_12.dll cublasLt64_12.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CUDA_BIN_DIR}/${DLL}" "${OUT_DIR}/"
    )
endforeach()
# 注意：CPU 版不复制 ggml-cuda.dll 和任何 CUDA 运行时 DLL

# Qt DLL
set(QT_BIN_DIR "${QT_DIR}/bin")
foreach(DLL Qt6Core.dll Qt6Gui.dll Qt6Svg.dll Qt6Widgets.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_BIN_DIR}/${DLL}" "${OUT_DIR}/"
    )
endforeach()

# Qt plugins
set(QT_PLUGIN_DIR "${QT_DIR}/plugins")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${QT_PLUGIN_DIR}/platforms/qwindows.dll" "${OUT_DIR}/platforms/"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/imageformats"
)
foreach(FMT qsvg.dll qjpeg.dll qtiff.dll qgif.dll qico.dll qwbmp.dll qwebp.dll qpdf.dll qicns.dll qtga.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_PLUGIN_DIR}/imageformats/${FMT}" "${OUT_DIR}/imageformats/"
    )
endforeach()

# office2md.exe
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/script/office2md.exe" "${OUT_DIR}/"
)
