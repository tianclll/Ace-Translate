# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

### GPU 版（有 NVIDIA 显卡 + CUDA 12.1）

```bash
# 一键编译
build_all.bat

# 或手动
cd build_all && cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### CPU 版（无 GPU）

```bash
# 一键编译（会自动复制 CMakeLists_cpu.txt → CMakeLists.txt）
build_cpu.bat

# 或手动
mkdir build_cpu && cd build_cpu
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Debug 编译

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
```

**注意：** `build/` 目录目前是 Release 配置。Debug 需把 Qt 的 `qwindowsd.dll` 复制到 `build/Debug/platforms/`。

### 依赖路径（写死在 CMakeLists.txt 中）

| 依赖 | 路径 |
|---|---|
| Qt 6.5.2 | `D:/Qt/6.5.2/msvc2019_64/` |
| OpenCV 4.8 | `D:/project/cpp/opencv/build/` |
| ONNXRuntime (GPU) | `D:/project/cpp/onnxruntime-win-x64-gpu-1.20.1/` |
| ONNXRuntime (CPU) | `D:/project/cpp/onnxruntime-win-x64-1.20.1/` |
| llama.cpp (GPU) | `tools/translator/TranslationApp/external/llama.cpp/build_gpu/` |
| llama.cpp (CPU) | `tools/translator/TranslationApp/external/llama.cpp/build/` |
| CUDA Toolkit | `C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.1/` |
| PDFium | `third_party/pdfium/`（已 vendored） |

### llama.cpp 预编译

```bash
# GPU 版
cd external/llama.cpp
cmake -B build_gpu -DGGML_CUDA=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_gpu --config Release

# CPU 版
cmake -B build -DGGML_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

编译产物在 `build_gpu/bin/Release/`（`llama.dll`, `ggml-cuda.dll`, `mtmd.dll` 等）。

### 运行

编译后 exe 在 `build_all/Release/AceTranslatePro.exe`（或 `build_cpu/Release/`）。
首次运行会在同目录生成 `config.json`。模型文件放在 `Release/models/` 下。

## Architecture

### 双构建模式

项目维护两套 CMake 配置：
- **`CMakeLists.txt`** — GPU 版：指向 `onnxruntime-win-x64-gpu`、`build_gpu` 的 llama、复制 `ggml-cuda.dll` + CUDA 运行时 DLL
- **`CMakeLists_cpu.txt`** — CPU 版：指向 `onnxruntime-win-x64`（CPU-only）、`build` 的 llama（无 CUDA）、不复制任何 CUDA DLL

`build_cpu.bat` 在编译前会执行：
```
copy /y CMakeLists_cpu.txt CMakeLists.txt
copy /y deps/translator/CMakeLists_cpu.txt deps/translator/CMakeLists.txt
copy /y deps/vlm/CMakeLists_cpu.txt deps/vlm/CMakeLists.txt
```

对应的 CPU 版子项目 CMakeLists 存放在 `deps/translator/CMakeLists_cpu.txt` 和 `deps/vlm/CMakeLists_cpu.txt`，它们不定义 `GGML_USE_CUDA`、不链接 `ggml-cuda.lib`。

### Layer Structure

```
ui/               → Qt6 Widgets GUI（入口、主窗口、面板、控件）
├── mainwindow.cpp/.h  — 主窗口（~2900 行），5 panels + 状态栏 + 托盘 + 热键 + 工作线程
├── floatwindow.cpp    — 划词翻译悬浮窗
├── regioncapture.cpp  — 全屏截图选区覆盖层
├── toast.cpp          — 顶部 Toast 通知组件
├── zoomablelabel.cpp  — 可缩放/拖拽的图片控件
├── splashscreen.cpp   — 启动画面
└── style.qss          — 全局样式表

src/              → C++ 核心逻辑
├── engines/           — DLLLoader + OCR/Translator/VLM/LayoutDetector 封装
├── processors/        — 文档处理管线（版面分析、OCR、VLM 推理）
├── modules/           — 业务模块（Text/File/Photo/ScreenshotTranslationModule）
├── core/              — GlobalEngineContext（单例引擎上下文）+ ConfigManager
└── utils/             — 图像处理、Markdown 生成、字体渲染、Office 转换

include/docmind/  → 公共接口头文件（mirrors src/ 结构）

deps/             → 引擎 DLL 源码（通过 add_subdirectory 集成）
├── ocr/               — PaddleOCR 封装（ONNXRuntime）
├── doclayout/         — PPDocLayoutV2 版面分析（ONNXRuntime）
├── vlm/               — VLM 多模态公式识别（llama.cpp）
├── translator/        — 文本翻译引擎（llama.cpp）
└── uvdoc/             — 文档图片矫正（ONNXRuntime）

script/           → Python 辅助工具（office2md 等）
third_party/      → vendor（PDFium）

tools/            → 独立工具链（llama.cpp 源码、office2md 等，不参与主项目编译）
```

### 关键类

| 类 | 文件 | 职责 |
|---|---|---|
| `MainWindow` | `ui/mainwindow.h/.cpp` | ~2900 行。导航栏 + QStackedWidget 管理 6 个面板，全局热键（Win32 `RegisterHotKey`），系统托盘，QStatusBar + 沙漏动画，工作线程管理 |
| `TranslateWorker` | `ui/mainwindow.h`（内联类） | QObject 子类，`moveToThread(QThread)`，5 种翻译模式：Text/File/Screenshot/Photo |
| `RegionCapture` | `ui/regioncapture.h/.cpp` | 全屏透明覆盖层，鼠标拖拽选区，返回 `cv::Mat` |
| `FloatTranslateWindow` | `ui/floatwindow.h/.cpp` | 无边框悬浮窗口，`Ctrl+Shift+C` 触发，显示划词翻译结果 |
| `ToastNotification` | `ui/toast.h/.cpp` | 顶部 Toast 通知，支持图标 + 自定义颜色，淡入淡出动画 |
| `ZoomableLabel` | `ui/zoomablelabel.h` | QLabel 子类，支持鼠标滚轮缩放 + 拖拽平移，存储 `originalFullPixmap_` |
| `GlobalEngineContext` | `include/docmind/core/GlobalEngineContext.hpp` | 单例，`std::call_once` 线程安全初始化，加载所有 DLL + 模型 |
| `ConfigManager` | `include/docmind/core/ConfigManager.hpp` | JSON 配置读写，`config.json`，单例 |
| `DropZoneWidget` | `ui/mainwindow.cpp` | QFrame 子类，支持拖放 + 点击上传 |

### 公共 API（DocumentEngine.h）

全局自由函数（`include/docmind/DocumentEngine.h`）：
- `translate_text(text, lang, max_tokens)` → 翻译文本
- `translate_screenshot_image(cv::Mat, lang, max_tokens)` → 截图翻译
- `process_file(path, ...)` → 文件翻译（按扩展名路由）
- `process_photo(path, ...)` → 图片翻译（OCR → 渲染）
- `process_image/pdf/markdown/txt(path, ...)` → 各格式文件翻译

### 工作线程（Threading）

每个翻译任务创建一个 `TranslateWorker` + `QThread`：
```
worker->moveToThread(thread);
connect(thread, &QThread::started, worker, &TranslateWorker::run);
connect(worker, &TranslateWorker::finished, this, &MainWindow::onWorkerFinished);
```
- `busy_`（QAtomicInt）防止并发翻译
- `fileCurrentIdx_` 追踪文件翻译列表的当前进度，支持自动翻译下一个

### 引擎 DLL 集成

5 个引擎 DLL 通过 `add_subdirectory(deps/*)` 集成到主项目编译，CMake `POST_BUILD` 自动复制：
- DLL 文件复制：`foreach(TGT ocr doclayout vlm_visual translator DocumentImageProcessor)`
- ONNXRuntime / OpenCV / PDFium / Qt / VC++ 运行时 DLL 也通过 POST_BUILD 复制
- GPU 版额外复制 `onnxruntime_providers_cuda.dll`、`ggml-cuda.dll`、CUDA 运行时 DLL

## UI Conventions

- **对象命名**：camelCase，如 `navBtn`、`primaryBtn`、`sectionTitle`、`card`
- **原生热键**：`RegisterHotKey` Win32 API + `nativeEventFilter` 处理
- **颜色方案**：`#F8FAFA` 背景，`#0B7C72` 主色（绿色），`#FFFFFF` 卡片，`#E8ECEF` 边框
- **面板构建**：所有面板在 `mainwindow.cpp` 中用工厂方法 `createXxxPanel()` 构建，添加到 `QStackedWidget`
- **样式表**：`ui/style.qss` 通过 Qt 资源 `:/style/style.qss` 加载
- **窗口大小**：默认 1113×620，最小 800×550
- **Toast 通知**：使用 `ToastNotification::show(parent, msg, duration, color)`，支持图标和自定义颜色
- **文件列表**：手动构建（非 QListWidget），QFrame 条目 + QVBoxLayout + QScrollArea
- **单实例限制**：`QSharedMemory` 检测重复启动

## 常见任务

- **加新面板**：在 `mainwindow.h/.cpp` 创建 `createXxxPanel()`，在 `setupUI()` 添加到 `QStackedWidget`
- **加全局热键**：在 `registerGlobalHotkeys()` 中 `RegisterHotKey`，在 `nativeEventFilter()` 处理
- **调整样式**：编辑 `ui/style.qss`
- **改构建配置**：GPU 版改 `CMakeLists.txt`，CPU 版改 `CMakeLists_cpu.txt` + `deps/*/CMakeLists_cpu.txt`
- **加翻译后端**：在 `src/modules/` 加新模块类，在 `include/docmind/modules/` 加头文件，在 `DocumentEngine.h` 暴露自由函数
- **改截图翻译流程**：`TranslateWorker::run()` 中 `case ScreenshotTranslate` 处理，`onCaptureComplete()` 触发
