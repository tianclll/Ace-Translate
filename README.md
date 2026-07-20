<div align="center">

<img src="ui/icons/LOGO.png" width="80" height="80" alt="AceTranslatePro Logo"/>

# AceTranslatePro

**一站式本地翻译工具**

> 文本翻译 · 划词翻译 · 截图翻译 · 图片翻译 · 文件翻译 · 语音输入  
> 纯离线 · 隐私安全 · GPU/CPU 双模式 · 多语言自动语音识别 · 多语言 TTS 朗读

<p align="center">
  <img src="https://img.shields.io/badge/Windows-10%2F11-blue?style=flat-square&logo=windows" alt="Windows"/>
  <img src="https://img.shields.io/badge/Qt-6.5.2-brightgreen?style=flat-square&logo=qt" alt="Qt"/>
  <img src="https://img.shields.io/badge/OpenCV-4.8-red?style=flat-square&logo=opencv" alt="OpenCV"/>
  <img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="License"/>
</p>

</div>

---

## ✨ 功能一览

<div align="center">

| 功能 | 说明 | 适用场景 |
|------|------|----------|
| 📝 **文本翻译** | 输入或粘贴文本，快速翻译 | 日常翻译需求 |
| 🖱️ **划词翻译** | 选中任意应用文字，`Ctrl+Shift+C` 弹出翻译 | 阅读外语文档 |
| 📷 **截图翻译** | 截取屏幕区域，自动 OCR + 翻译，支持多种语言 | 图片中的文字 |
| 🖼️ **图片翻译** | 上传图片，翻译后渲染回图片 | 漫画、海报、菜单 |
| 📄 **文件翻译** | PDF/Word/Excel/PPT/MD/TXT 批量文档翻译 | 批量文档处理 |
| 🎤 **语音输入** | 点击麦克风按钮录音，自动识别语音语言并转文字 | 语音输入 |
| 🔊 **朗读** | 支持中文/英文/日文/韩文/泰米尔/印地语等多语言 TTS 朗读 | 听译文发音 |

</div>

---

## 🖼️ 界面预览

> *截图待补充——替换以下占位图为实际界面截图*

![主界面](docs/images/main_window.png)

## 📸 翻译效果

> *效果图待补充——替换以下占位图为实际翻译效果截图*

### 文本翻译

![文本翻译效果](docs/images/demo_text.png)

### 截图翻译

![截图翻译效果](docs/images/demo_screenshot.png)

### 图片翻译

![图片翻译效果](docs/images/demo_photo.png)

### 文件翻译

![文件翻译效果](docs/images/demo_file.png)

---

## 📋 系统要求

- **操作系统**：Windows 10/11 64-bit
- **CPU**：支持 AVX2 指令集（2013 年后 Intel/AMD 处理器均可）
- **GPU（可选）**：NVIDIA 显卡 + CUDA 12.1，加速 OCR/翻译/ASR 推理
- **内存**：建议 16GB 以上
- **硬盘**：约 15GB（含模型文件）

---

## 🚀 快速开始

### 下载预编译版本

> [Releases](https://github.com/yourusername/AceTranslatePro/releases) 页面提供 GPU 版和 CPU 版压缩包，下载解压即可运行。

### 从源码编译

#### 🔧 前置依赖

| 依赖 | 版本 | 下载 |
|------|------|------|
| Visual Studio 2022 | 17.x | [下载](https://visualstudio.microsoft.com/)（需"使用 C++ 的桌面开发"） |
| CMake | ≥ 3.10 | [下载](https://cmake.org/download/) |
| Qt | 6.5.2 MSVC 2019 64-bit | [下载](https://www.qt.io/download-open-source) |
| OpenCV | 4.8 | [下载](https://opencv.org/releases/) |
| ONNXRuntime (GPU) | 1.20.1 (CUDA 12.1) | [下载](https://github.com/microsoft/onnxruntime/releases) |
| ONNXRuntime (CPU) | 1.20.1 | [下载](https://github.com/microsoft/onnxruntime/releases) |
| CUDA Toolkit | 12.1 | [下载](https://developer.nvidia.com/cuda-toolkit)（仅 GPU 版需要） |
| Python | 3.8+ | [下载](https://www.python.org/downloads/)（仅编译 office2md 需要） |

#### 1. 编译 llama.cpp

llama.cpp 需提前编译好，GPU 版依赖 CUDA Toolkit 12.1。

```bash
# 从 llama.cpp 官方仓库下载源码（master 分支）
# https://github.com/ggml-org/llama.cpp

# GPU 版
cd external/llama.cpp
cmake -B build_gpu -DGGML_CUDA=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_gpu --config Release

# CPU 版
cmake -B build -DGGML_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

编译产物在 `build_gpu/bin/Release/`（GPU）或 `build/bin/Release/`（CPU）。

#### 2. 编译主程序

```bash
# 克隆仓库
git clone https://github.com/yourusername/AceTranslatePro.git
cd AceTranslatePro
```

<details>
<summary><b>🖥️ GPU 版编译（有 NVIDIA 显卡）</b></summary>

```bash
# 一键编译（推荐）
双击 build_all.bat

# 或手动执行
mkdir build_all && cd build_all
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

</details>

<details>
<summary><b>💻 CPU 版编译（无独立显卡）</b></summary>

```bash
# 一键编译（推荐）
双击 build_cpu.bat

# 或手动执行
mkdir build_cpu && cd build_cpu
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

</details>

#### 📁 模型文件

编译完成后，将模型文件放入 `build_all/Release/models/`（或 `build_cpu/Release/models/`）目录：

```
models/
├── translation/                     # 翻译模型（需自行准备）
│   ├── Hy-MT2-1.8B-Q4_K_M.gguf      # 翻译模型（默认）
│   ├── Hy-MT2-1.8B-Q6_K.gguf        # 翻译模型（更高精度）
│   ├── HY-MT1.5-1.8B-Q4_K_M.gguf    # 翻译模型（旧版）
│   └── HY-MT1.5-1.8B-Q6_K.gguf      # 翻译模型（旧版高精度）
├── VLM/
│   └── PaddleOCR-VL-1.6-GGUF*.gguf  # VLM 多模态公式识别模型
├── layout/
│   └── pp_doclayoutv2.onnx          # 版面分析模型
├── ocr/
│   ├── tiny/                        # OCR 模型（轻量，默认）
│   │   ├── det/det.onnx             # 文本检测
│   │   ├── rec/rec.onnx             # 文字识别
│   │   └── cls/cls.onnx             # 方向分类
│   ├── small/                       # OCR 模型（中等）
│   └── medium/                      # OCR 模型（高精度）
├── ASR/
│   ├── model_quant.onnx             # 语音识别模型（SenseVoiceSmall）
│   ├── am.mvn                       # CMVN 归一化文件（推荐）
│   └── tokens.json                  # 语音 token 映射
└── uvdoc/
    └── UVDoc_grid.onnx              # 图片矫正模型
```

> **模型下载**：[Hugging Face 🤗](https://huggingface.co/your-username/your-repo)（待上传）

> **注意**：本项目为追求轻量化，所有模型均使用量化版本（翻译 Q4_K_M、OCR tiny、小型 VLM 等），在常见场景下表现良好。如果您有更高精度、更全语言覆盖的需求，欢迎联系我定制。

---

## ⚙️ 配置

程序首次运行会在同目录生成 `config.json`，也可以在设置面板中修改：

**常用设置**：

- **默认语言**：设置翻译目标语言（35 种语言可选，所有面板统一）
- **OCR 模型大小**：tiny（轻量）→ medium（高精度）
- **翻译模型**：自动扫描 `models/translation/` 目录，列出所有 `.gguf` 文件
- **启动加载引擎**：选择开机时自动加载哪些引擎，关闭可减少启动时间。默认仅加载翻译引擎

**GPU 设置**（设置 → GPU 设置）：

- **总开关**：一键启用/关闭所有引擎的 GPU 加速
- **独立开关**：分别为 OCR、版面分析、公式识别、图片矫正、翻译引擎、语音识别设置

> 修改 GPU 设置和模型大小后需重启应用生效。

---

## ⌨️ 快捷键

| 快捷键 | 功能 | 自定义 |
|--------|------|--------|
| `Ctrl+Shift+C` | 划词翻译 | ✅ 可在浮窗配置中修改 |
| `Ctrl+Shift+Z` | 截图翻译 | ✅ 可在截图面板中修改 |

---

## 🏗️ 项目架构

```
AceTranslatePro/
├── 📜 build_all.bat          # GPU 版编译脚本
├── 📜 build_cpu.bat          # CPU 版编译脚本（自动替换 CMakeLists）
├── 📜 CMakeLists.txt         # GPU 版构建配置
├── 📜 CMakeLists_cpu.txt     # CPU 版构建配置
│
├── 📂 ui/                    # Qt6 图形界面
│   ├── mainwindow.cpp        # 主窗口（6 面板 + 状态栏 + 托盘 + 热键 + 语音）
│   ├── floatwindow.cpp       # 划词翻译悬浮窗
│   ├── regioncapture.cpp     # 截图选区
│   ├── toast.cpp             # 顶部通知组件
│   ├── zoomablelabel.cpp     # 可缩放图片控件
│   └── style.qss             # 全局样式表
│
├── 📂 src/                   # C++ 核心代码
│   ├── engines/              # DLL 封装（OCR/VLM/Translator/Layout/ASR/DocProc）
│   ├── processors/           # 文档处理管线
│   ├── modules/              # 业务逻辑模块
│   ├── core/                 # 引擎上下文 & 配置管理
│   └── utils/                # 工具函数
│
├── 📂 deps/                  # 引擎 DLL 源码（add_subdirectory）
│   ├── asr/                  # SenseVoice 语音识别（ONNXRuntime + OpenCV）
│   │   ├── SenseVoiceEngine  # ONNX 推理 + CTC 解码 + CMVN
│   │   ├── Fbank             # FBank 特征提取（OpenCV FFT, LFR）
│   │   ├── CMVN              # CMVN 归一化（am.mvn 解析）
│   │   └── AudioCapture      # waveIn API 录音
│   ├── ocr/                  # PaddleOCR（ONNXRuntime）
│   ├── doclayout/            # 版面分析（ONNXRuntime）
│   ├── vlm/                  # VLM 多模态公式识别（llama.cpp）
│   ├── translator/           # 翻译引擎（llama.cpp）
│   └── uvdoc/                # 图片矫正（ONNXRuntime）
│
├── 📂 script/                # 辅助工具
│   └── office2md/            # Office → Markdown 转换
│
└── 📂 third_party/           # 第三方库
    ├── pdfium/               # PDF 解析引擎
    └── nlohmann/             # JSON 解析库
```

---

## 🧩 技术栈

<p align="center">
  <img src="https://img.shields.io/badge/Qt-6.5.2-41CD52?style=flat-square&logo=qt" />
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B" />
  <img src="https://img.shields.io/badge/OpenCV-4.8-5C3EE8?style=flat-square&logo=opencv" />
  <img src="https://img.shields.io/badge/ONNX_Runtime-1.20-005BED?style=flat-square" />
  <img src="https://img.shields.io/badge/llama.cpp-latest-FF6F00?style=flat-square" />
  <img src="https://img.shields.io/badge/CUDA-12.1-76B900?style=flat-square&logo=nvidia" />
</p>

| 组件 | 技术 |
|------|------|
| 🖥️ 界面 | Qt 6.5.2 Widgets |
| 🖼️ 图像处理 | OpenCV 4.8 |
| 🔍 OCR | PaddleOCR (ONNXRuntime) |
| 📐 版面分析 | PPDocLayoutV2 (ONNXRuntime) |
| 🧮 公式识别 | VLM 多模态模型 (llama.cpp) |
| 🌐 翻译 | 本地 LLM (llama.cpp) |
| 🎤 语音识别 | SenseVoiceSmall (ONNXRuntime + OpenCV FFT) |
| 🔊 语音合成 | Windows SAPI（多语言自动匹配） |
| 📄 文档解析 | PDFium + office2md (Python) |

---

## 📄 许可证

本项目基于 MIT 许可证开源。详见 [LICENSE](LICENSE) 文件。

---

<div align="center">

**Made with ❤️**

如需更高精度、更多语言覆盖的定制模型或服务器版本，请联系微信：`AceTranslatePro`

</div>
