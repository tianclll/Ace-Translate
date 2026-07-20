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
# 一键编译（会自动复制所有 CMakeLists_cpu.txt → CMakeLists.txt）
build_cpu.bat

# 或手动
mkdir build_cpu && cd build_cpu
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

**注意：** GPU 和 CPU 版共用同一份 `CMakeLists.txt`/`deps/*/CMakeLists.txt`。CPU 版编译前 `build_cpu.bat` 会自动把 `CMakeLists_cpu.txt` 和 `deps/*/CMakeLists_cpu.txt` 复制覆盖。编译 GPU 版时先确保不被 CPU 版覆盖（`git checkout CMakeLists.txt deps/*/CMakeLists.txt`）。

### Debug 编译

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
```

### 依赖路径（写死在 CMakeLists 中）

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
copy /y deps/asr/CMakeLists_cpu.txt deps/asr/CMakeLists.txt
```

对应的 CPU 版子项目 CMakeLists 存放在 `deps/*/CMakeLists_cpu.txt`，它们不定义 `GGML_USE_CUDA`、不链接 `ggml-cuda.lib`。

### Layer Structure

```
ui/               → Qt6 Widgets GUI（入口、主窗口、面板、控件）
├── mainwindow.cpp/.h  — 主窗口（~3000 行），6 panels + 状态栏 + 托盘 + 热键 + 工作线程
├── floatwindow.cpp    — 划词翻译悬浮窗
├── regioncapture.cpp  — 全屏截图选区覆盖层
├── toast.cpp          — 顶部 Toast 通知组件（支持图标、链接）
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
├── uvdoc/             — 文档图片矫正（ONNXRuntime）
└── asr/               — 语音识别（SenseVoice + ONNXRuntime）
    ├── SenseVoiceEngine.cpp/.h  — ONNX 推理、CTC 解码、CMVN
    ├── SenseVoiceDLL.cpp        — DLL 导出接口
    ├── Fbank.cpp/.h             — FBank 特征提取（OpenCV FFT + LFR）
    ├── CMVN.cpp/.h              — CMVN 归一化（am.mvn 解析）
    └── AudioCapture.cpp/.h      — waveIn API 录音

script/           → Python 辅助工具（office2md 等）
third_party/      → vendor（PDFium、nlohmann/json）
```

### 关键类

| 类 | 文件 | 职责 |
|---|---|---|
| `MainWindow` | `ui/mainwindow.h/.cpp` | ~3000 行。导航栏 + QStackedWidget 管理 6 个面板，全局热键（Win32 `RegisterHotKey`），系统托盘，QStatusBar + 沙漏动画，工作线程管理，多语言 TTS 朗读（Windows SAPI），语音输入（麦克风按钮） |
| `TranslateWorker` | `ui/mainwindow.h`（内联类） | QObject 子类，`moveToThread(QThread)`，6 种翻译模式：Text/File/Screenshot/Photo |
| `RegionCapture` | `ui/regioncapture.h/.cpp` | 全屏透明覆盖层，鼠标拖拽选区，返回 `cv::Mat` |
| `FloatTranslateWindow` | `ui/floatwindow.h/.cpp` | 无边框悬浮窗口，`Ctrl+Shift+C` 触发，显示划词翻译结果 |
| `ToastNotification` | `ui/toast.h/.cpp` | 顶部 Toast 通知，支持图标 + 自定义颜色 + 链接，淡入淡出动画 |
| `ZoomableLabel` | `ui/zoomablelabel.h` | QLabel 子类，支持鼠标滚轮缩放 + 拖拽平移 |
| `GlobalEngineContext` | `include/docmind/core/GlobalEngineContext.hpp` | 单例，`std::call_once` 线程安全初始化，按 `enable_*` 配置选择加载引擎 |
| `ConfigManager` | `include/docmind/core/ConfigManager.hpp` | JSON 配置读写，`config.json`，单例，支持点号嵌套访问 |
| `SenseVoiceEngine` | `deps/asr/src/SenseVoiceEngine.h/.cpp` | SenseVoice ONNX 推理：FBank→CMVN→LFR→ONNX→CTC→SentencePiece |
| `Fbank` | `deps/asr/src/Fbank.h/.cpp` | FBank 特征提取（汉明窗、OpenCV FFT、Mel filter bank、LFR 跳帧） |
| `CMVN` | `deps/asr/src/CMVN.h/.cpp` | 解析 Kaldi 格式 am.mvn，做归一化 |

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
- `fileCurrentIdx_` 追踪文件翻译列表的当前进度
- 语音输入使用 `QThread::create()` 启动后台线程，`waveIn` 录音 + `asrRecognize` 识别

### 引擎 DLL 集成

6 个引擎 DLL 通过 `add_subdirectory(deps/*)` 集成，CMake `POST_BUILD` 自动复制。

| DLL | 依赖 | 功能 |
|-----|------|------|
| `ocr.dll` | ONNXRuntime、OpenCV | PaddleOCR 文字识别 |
| `doclayout.dll` | ONNXRuntime | 版面分析 |
| `vlm_visual.dll` | llama.cpp | 公式识别 |
| `translator.dll` | llama.cpp | 文本翻译 |
| `DocumentImageProcessor.dll` | ONNXRuntime | 图片矫正 |
| `asr.dll` | ONNXRuntime、OpenCV | 语音识别（SenseVoice） |

### ASR 语音识别流程

```
PCM(16kHz 16-bit mono) → FBank(80-dim) → CMVN → LFR(7帧拼1帧, frame_skip=6) → 560-dim
→ ONNX(SenseVoice) → CTC logits → CTC解码(blank=vocab-1) → SentencePiece拼接 → 文字
```

- 测试模式：`Shift`+点击麦克风按钮，从 `test.wav` 读取音频
- CMVN 文件：`models/ASR/am.mvn`（Kaldi 格式，可选但推荐）
- 模型输入：`speech[float]`, `speech_lengths[int32]`, `language[int32]`, `textnorm[int32]`
- 模型输出：`ctc_logits[float]`, `encoder_out_lens[int64]`

### TTS 朗读（Windows SAPI）

- Unicode 范围检测语言：CJK(0x4E00-0x9FFF)、韩文(0xAC00-0xD7AF)、日文(0x3040-0x30FF)、泰米尔等 20+ 语系
- LCID 匹配 → 名称匹配（"Chinese"、"Hui"、"Xiao"、"中文"） 两级匹配策略
- 同步模式（`SPF_DEFAULT`），避免 ISpVoice 生命周期问题

## UI Conventions

- **对象命名**：camelCase，如 `navBtn`、`primaryBtn`、`sectionTitle`、`card`
- **原生热键**：`RegisterHotKey` Win32 API + `nativeEventFilter` 处理
- **颜色方案**：`#F8FAFA` 背景，`#0B7C72` 主色（绿色），`#FFFFFF` 卡片，`#E8ECEF` 边框
- **面板构建**：所有面板在 `mainwindow.cpp` 中用工厂方法 `createXxxPanel()` 构建
- **样式表**：`ui/style.qss` 通过 Qt 资源 `:/style/style.qss` 加载
- **窗口大小**：默认 1113×620，最小 800×550
- **Toast 通知**：`ToastNotification::show(parent, msg, duration, color, iconPath, actionText, actionUrl)`
- **语言下拉框**：35 种中文名称语言（从"中文"到"粤语"），目标语言映射到模型理解的名称
- **设置面板**：默认语言、OCR 模型大小（tiny/medium/small）、翻译模型（动态扫描 `models/translation/` 目录）、引擎加载开关、GPU 开关
- **麦克风按钮**：QPushButton，点击录音/停止录音，hover 禁用 `QEvent::ToolTip`（全局事件过滤器）

## 常见任务

- **加新面板**：在 `mainwindow.h/.cpp` 创建 `createXxxPanel()`，在 `setupUI()` 添加到 `QStackedWidget`
- **加全局热键**：在 `registerGlobalHotkeys()` 中 `RegisterHotKey`，在 `nativeEventFilter()` 处理；修改设置对话框时先 `UnregisterHotKey` 防止误触
- **调整样式**：编辑 `ui/style.qss`
- **改构建配置**：GPU 版改 `CMakeLists.txt` + `deps/*/CMakeLists.txt`，CPU 版改 `CMakeLists_cpu.txt` + `deps/*/CMakeLists_cpu.txt`，两套必须同步
- **加翻译后端**：在 `src/modules/` 加新模块类，在 `include/docmind/modules/` 加头文件，在 `DocumentEngine.h` 暴露自由函数
- **改截图翻译流程**：`TranslateWorker::run()` 中 `case ScreenshotTranslate` 处理，`onCaptureComplete()` 触发
- **扩展 TTS 语言**：在 `speakText()` 的 Unicode 范围判断和 LCID 映射中添加新语种
- **修改设置页**：在 `createSettingsPanel()` 中增删配置组，config 读写通过 `ConfigManager::getNestedJson/setNestedString/setNestedBool`
- **改 ASR 前处理**：`deps/asr/src/Fbank.cpp`（FBank 参数、LFR 跳帧）、`CMVN.cpp`（am.mvn 加权方式）
