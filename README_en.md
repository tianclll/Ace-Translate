<div align="center">
<img src="ui/icons/LOGO.png" width="100" height="100" alt="AceTranslatePro Logo"/>

# AceTranslatePro

**All-in-One Local Translation Tool**

> Text Translation · Selection Translation · Screenshot Translation · Image Translation · File Translation · Voice Input  
> Fully Offline · Privacy Safe · GPU/CPU Dual Mode · Multilingual ASR · Multilingual TTS

<p align="center">
  <img src="https://img.shields.io/badge/Windows-10%2F11-blue?style=flat-square&logo=windows" alt="Windows"/>
  <img src="https://img.shields.io/badge/Qt-6.5.2-brightgreen?style=flat-square&logo=qt" alt="Qt"/>
  <img src="https://img.shields.io/badge/OpenCV-4.8-red?style=flat-square&logo=opencv" alt="OpenCV"/>
  <img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="License"/>
</p>

</div>

---

## ✨ Features

<div align="center">

| Feature | Description | Use Case |
|---------|-------------|-----------|
| 📝 **Text Translation** | Type or paste text, translate instantly | Everyday translation needs |
| 🖱️ **Selection Translation** | Select text in any app, `Ctrl+Shift+C` to translate | Reading foreign documents |
| 📷 **Screenshot Translation** | Capture screen area, auto OCR + translate, multi-language | Text in images |
| 🖼️ **Image Translation** | Upload image, translate and render back onto image | Comics, posters, menus |
| 📄 **File Translation** | Batch document translation for PDF/Word/Excel/PPT/MD/TXT | Batch document processing |
| 🎤 **Voice Input** | Click mic button to record, auto speech recognition to text | Hands-free input |
| 🔊 **Read Aloud** | Multilingual TTS support (CN/EN/JP/KO/TA/HI & more) | Listen to translation |
| 🌐 **Multilingual UI** | Switch between Chinese/English/Japanese, draggable nav bar | Multilingual users |

</div>

---

## 🖼️ Preview

> *Click images to enlarge*

![Main Interface](docs/images/text_translate.png)

## 📸 Translation Showcase

> *Click images to enlarge*

### Text Translation

![Text Translation](docs/images/text_translate.png)

### Selection Translation

![Selection Translation](docs/images/selection_translate1.png)

![Selection Translation 2](docs/images/selection_translate2.png)

### Screenshot Translation

![Screenshot Translation](docs/images/ScreenShot_translate.png)

### Image Translation

![Image Translation](docs/images/image_translate.png)

### File Translation

![File Translation](docs/images/document_translation1.png)
<img src="docs/images/document_translate2.jpg" width="48%"> <img src="docs/images/document_translate3.png" width="48%">

---

## 📋 System Requirements

- **OS**: Windows 10/11 64-bit
- **CPU**: AVX2 support (Intel/AMD processors from 2013+)
- **GPU (optional)**: NVIDIA GPU + CUDA 12.1 for accelerated OCR/Translation/ASR
- **RAM**: 16GB+ recommended
- **Storage**: ~15GB (including model files)

---

## 🚀 Quick Start

### Download Pre-built Release

> Pre-built GPU and CPU packages are available on the [Releases](https://github.com/tianclll/Ace-Translate/releases) page. Download and run.

### Build from Source

#### 🔧 Prerequisites

| Dependency | Version | Download |
|-----------|---------|----------|
| Visual Studio 2022 | 17.x | [Download](https://visualstudio.microsoft.com/) (requires "Desktop development with C++") |
| CMake | ≥ 3.10 | [Download](https://cmake.org/download/) |
| Qt | 6.5.2 MSVC 2019 64-bit | [Download](https://www.qt.io/download-open-source) |
| OpenCV | 4.8 | [Download](https://opencv.org/releases/) |
| ONNXRuntime (GPU) | 1.20.1 (CUDA 12.1) | [Download](https://github.com/microsoft/onnxruntime/releases) |
| ONNXRuntime (CPU) | 1.20.1 | [Download](https://github.com/microsoft/onnxruntime/releases) |
| CUDA Toolkit | 12.1 | [Download](https://developer.nvidia.com/cuda-toolkit) (GPU only) |
| Python | 3.8+ | [Download](https://www.python.org/downloads/) (only needed for office2md) |

#### 1. Build llama.cpp

llama.cpp must be built in advance. GPU build requires CUDA Toolkit 12.1.

```bash
# Clone llama.cpp source (master branch)
# https://github.com/ggml-org/llama.cpp

# GPU build
cd external/llama.cpp
cmake -B build_gpu -DGGML_CUDA=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_gpu --config Release

# CPU build
cmake -B build -DGGML_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Outputs: `build_gpu/bin/Release/` (GPU) or `build/bin/Release/` (CPU).

#### 2. Build Main Application

```bash
git clone https://github.com/yourusername/AceTranslatePro.git
cd AceTranslatePro
```

<details>
<summary><b>🖥️ GPU Build (NVIDIA GPU)</b></summary>

```bash
# One-click build (recommended)
Double-click build_all.bat

# Or manually
mkdir build_all && cd build_all
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

</details>

<details>
<summary><b>💻 CPU Build (No GPU)</b></summary>

```bash
# One-click build (recommended)
Double-click build_cpu.bat

# Or manually
mkdir build_cpu && cd build_cpu
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

</details>

#### 📁 Model Files

After building, place model files in `build_all/Release/models/` (or `build_cpu/Release/models/`):

```
models/
├── translation/                     # Translation models
│   ├── Hy-MT2-1.8B-Q4_K_M.gguf      # Default translation model
│   ├── Hy-MT2-1.8B-Q6_K.gguf        # Higher precision
│   ├── HY-MT1.5-1.8B-Q4_K_M.gguf    # Legacy model
│   └── HY-MT1.5-1.8B-Q6_K.gguf      # Legacy high precision
├── VLM/
│   └── PaddleOCR-VL-1.6-GGUF*.gguf  # VLM formula recognition model
├── layout/
│   └── pp_doclayoutv2.onnx          # Layout analysis model
├── ocr/
│   ├── tiny/                        # Lightweight (default)
│   │   ├── det/det.onnx
│   │   ├── rec/rec.onnx
│   │   └── cls/cls.onnx
│   ├── small/
│   └── medium/
├── ASR/
│   ├── model_quant.onnx             # Speech recognition (SenseVoiceSmall)
│   ├── am.mvn                       # CMVN normalization file
│   └── tokens.json
└── uvdoc/
    └── UVDoc_grid.onnx              # Image correction model
```

> **Download models**: [Hugging Face 🤗](https://huggingface.co/tianclll/AceTranslatePro-models)

> **Note**: This project uses quantized models for lightweight deployment (Q4_K_M translation, tiny OCR, small VLM). Performance is solid for common use cases. Contact me for higher precision or broader language coverage.

---

## ⚙️ Configuration

A `config.json` is generated on first run. Settings can also be modified in the Settings panel.

**Common Settings:**

- **Default Language**: Set the translation target language (35 languages available, unified across all panels)
- **UI Language**: Switch interface language (Chinese / English / Japanese), takes effect after restart
- **OCR Model Size**: Tiny (lightweight) → Medium (high precision)
- **Translation Model**: Auto-scans `models/translation/` directory, lists all `.gguf` files
- **Load Engines on Startup**: Choose which engines load at startup, disable to reduce startup time. Only translation engine is loaded by default.

**GPU Settings** (Settings → GPU Settings):

- **Master Switch**: Enable/disable GPU acceleration for all engines at once
- **Per-engine Switches**: Individually set GPU for OCR, Layout, Formula Recognition, Image Correction, Translation, Speech Recognition

> Changes to GPU settings and model size require app restart.

---

## ⌨️ Hotkeys

| Hotkey | Function | Customizable |
|--------|----------|-------------|
| `Ctrl+Shift+C` | Selection Translation | ✅ In float window settings |
| `Ctrl+Shift+Z` | Screenshot Translation | ✅ In screenshot panel |

---

## 🏗️ Project Architecture

```
AceTranslatePro/
├── 📜 build_all.bat          # GPU build script
├── 📜 build_cpu.bat          # CPU build script (auto-replaces CMakeLists)
├── 📜 CMakeLists.txt         # GPU build config
├── 📜 CMakeLists_cpu.txt     # CPU build config
│
├── 📂 ui/                    # Qt6 GUI
│   ├── mainwindow.cpp        # Main window (6 panels + status bar + tray + hotkeys + voice)
│   ├── floatwindow.cpp       # Selection translation popup
│   ├── regioncapture.cpp     # Screenshot selection overlay
│   ├── toast.cpp             # Top toast notification
│   ├── zoomablelabel.cpp     # Zoomable image widget
│   ├── style.qss             # Global stylesheet
│   └── i18n/                 # Translation files (.ts/.qm)
│
├── 📂 translations/           # Qt translation files (zh_CN/en_US/ja_JP)
│
├── 📂 src/                   # C++ core logic
│   ├── engines/              # DLL wrappers (OCR/VLM/Translator/Layout/ASR/DocProc)
│   ├── processors/           # Document processing pipeline
│   ├── modules/              # Business logic modules
│   ├── core/                 # Engine context & config management
│   └── utils/                # Utility functions
│
├── 📂 deps/                  # Engine DLL source (add_subdirectory)
│   ├── asr/                  # SenseVoice (ONNXRuntime + OpenCV)
│   ├── ocr/                  # PaddleOCR (ONNXRuntime)
│   ├── doclayout/            # Layout analysis (ONNXRuntime)
│   ├── vlm/                  # VLM multimodal (llama.cpp)
│   ├── translator/           # Translation engine (llama.cpp)
│   └── uvdoc/                # Image correction (ONNXRuntime)
│
├── 📂 script/                # Helper tools
│   └── office2md/            # Office → Markdown conversion
│
└── 📂 third_party/           # Third-party libraries
    ├── pdfium/               # PDF parsing engine
    └── nlohmann/             # JSON parsing
```

---

## 🧩 Tech Stack

<p align="center">
  <img src="https://img.shields.io/badge/Qt-6.5.2-41CD52?style=flat-square&logo=qt" />
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B" />
  <img src="https://img.shields.io/badge/OpenCV-4.8-5C3EE8?style=flat-square&logo=opencv" />
  <img src="https://img.shields.io/badge/ONNX_Runtime-1.20-005BED?style=flat-square" />
  <img src="https://img.shields.io/badge/llama.cpp-latest-FF6F00?style=flat-square" />
  <img src="https://img.shields.io/badge/CUDA-12.1-76B900?style=flat-square&logo=nvidia" />
</p>

| Component | Technology |
|-----------|-----------|
| 🖥️ UI | Qt 6.5.2 Widgets (CN/EN/JP trilingual, draggable nav bar) |
| 🖼️ Image Processing | OpenCV 4.8 |
| 🔍 OCR | PaddleOCR (ONNXRuntime) |
| 📐 Layout Analysis | PPDocLayoutV2 (ONNXRuntime) |
| 🧮 Formula Recognition | VLM Multimodal model (llama.cpp) |
| 🌐 Translation | Local LLM (llama.cpp) |
| 🎤 Speech Recognition | SenseVoiceSmall (ONNXRuntime + OpenCV FFT) |
| 🔊 TTS | Windows SAPI (auto language matching) |
| 📄 Document Parsing | PDFium + office2md (Python) |

---

## 📄 License

This project is open-sourced under the MIT License. See [LICENSE](LICENSE) for details.

> 📌 The legacy Python version has been moved to the [`archive/old-python-version`](https://github.com/tianclll/Ace-Translate/tree/archive/old-python-version) branch.

---

<div align="center">

**Made with ❤️**

For custom higher-precision models or server editions, contact WeChat: `kriswu1106tc`

</div>
