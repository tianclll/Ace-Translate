<div align="center">

<img src="ui/icons/LOGO.png" width="80" height="80" alt="AceTranslatePro Logo"/>

# AceTranslatePro

**一站式本地翻译工具**

> 文本翻译 · 划词翻译 · 截图翻译 · 图片翻译 · 文件翻译  
> 纯离线 · 隐私安全 · GPU/CPU 双模式

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
| 📝 **文本翻译** | 输入或粘贴文本，翻译到 118 种语言 | 日常翻译需求 |
| 🖱️ **划词翻译** | 选中任意应用文字，`Ctrl+Shift+C` 弹出翻译 | 阅读外语文档 |
| 📷 **截图翻译** | 截取屏幕区域，自动 OCR + 翻译 | 图片中的文字 |
| 🖼️ **图片翻译** | 上传图片，翻译后渲染回图片 | 漫画、海报、菜单 |
| 📄 **文件翻译** | PDF/Word/Excel/PPT/MD/TXT | 批量文档翻译 |

</div>

## 🖼️ 界面预览

<details open>
<summary><b>主界面</b></summary>

> *截图待补充*

</details>

<details>
<summary><b>各功能面板</b></summary>

> *截图待补充*

</details>

---

## 📋 系统要求

- **操作系统**：Windows 10/11 64-bit
- **CPU**：支持 AVX2 指令集（2013 年后 Intel/AMD 处理器均可）
- **GPU（可选）**：NVIDIA 显卡 + CUDA 12.1，加速 OCR/翻译推理
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

llama.cpp 需提前编译好，依赖 CUDA Toolkit 12.1（GPU 版需要）。

```bash
# 从 llama.cpp 官方仓库下载源码（master 分支）
# https://github.com/ggml-org/llama.cpp
# 或使用项目自带的 external 源码

# 配置（GPU 版）
cmake -B build_gpu -DGGML_CUDA=ON -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build_gpu --config Release
```

编译完成后，`build_gpu/bin/Release/` 目录下会生成以下 DLL：

```
llama.dll  llama-common.dll  ggml.dll  ggml-base.dll
ggml-cpu.dll  ggml-cuda.dll  mtmd.dll
```

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
├── translation/
│   └── HY-MT2-1.8B-Q4_K_M.gguf         # 翻译模型 (~1.8GB)
├── VLM/
│   ├── PaddleOCR-VL-1.6-GGUF.gguf        # VLM 模型
│   └── PaddleOCR-VL-1.6-GGUF-mmproj.gguf # mmproj 文件
├── layout/
│   └── pp_doclayoutv2.onnx              # 版面分析模型
├── ocr/
│   ├── det/                              # 文本检测模型
│   ├── rec/                              # 文字识别模型
│   └── cls/                              # 方向分类模型
└── uvdoc/
    └── UVDoc_grid.onnx                   # 图片矫正模型
```

> **模型下载**：[Hugging Face 🤗](https://huggingface.co/your-username/your-repo)（待上传）

> **注意**：本项目为追求轻量化，所有模块均使用了量化/轻量模型（翻译 Q4_K_M、OCR mobile 版、小型 VLM 等），翻译质量和识别精度可能不如全精度或大型模型。如果您有特定场景的更高精度需求（专业领域的翻译模型、高精度 OCR、大型 VLM 等），欢迎联系我定制。

---

## ⚙️ 配置

程序首次运行会在同目录生成 `config.json`，也可以在设置面板中修改：

<!-- 配置截图 -->
> *设置面板截图待补充*

**GPU 设置**（设置 → GPU 设置）：

- **总开关**：一键启用/关闭所有引擎的 GPU 加速
- **独立开关**：分别为 OCR、版面分析、公式识别、图片矫正、翻译引擎设置

> 修改 GPU 设置后需重启应用生效。

---

## ⌨️ 快捷键

| 快捷键 | 功能 | 自定义 |
|--------|------|--------|
| `Ctrl+Shift+C` | 划词翻译 | ✅ 可在设置中修改 |
| `Ctrl+Shift+Z` | 截图翻译 | ✅ 可在设置中修改 |

---

## 🏗️ 项目架构

```
AceTranslatePro/
├── 📜 build_all.bat          # GPU 版编译脚本
├── 📜 build_cpu.bat          # CPU 版编译脚本
├── 📜 CMakeLists.txt         # GPU 版构建配置
├── 📜 CMakeLists_cpu.txt     # CPU 版构建配置
│
├── 📂 ui/                    # Qt6 图形界面
│   ├── mainwindow.cpp        # 主窗口
│   ├── floatwindow.cpp       # 划词翻译悬浮窗
│   ├── regioncapture.cpp     # 截图选区
│   └── style.qss             # 样式表
│
├── 📂 src/                   # C++ 核心代码
│   ├── engines/              # DLL 封装（OCR/VLM/Translator/Layout）
│   ├── processors/           # 文档处理管线
│   ├── modules/              # 业务逻辑模块
│   ├── core/                 # 引擎上下文 & 配置管理
│   └── utils/                # 工具函数
│
├── 📂 include/docmind/       # 公共接口头文件
│
├── 📂 deps/                  # 引擎 DLL 源码
│   ├── ocr/                  # PaddleOCR 封装
│   ├── doclayout/            # 版面分析
│   ├── vlm/                  # VLM 公式识别
│   ├── translator/           # 翻译引擎
│   └── uvdoc/                # 图片矫正
│
├── 📂 script/                # 辅助工具
│   └── office2md/            # Office → Markdown 转换
│
└── 📂 third_party/           # 第三方库
    └── pdfium/               # PDF 解析引擎
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
| 📄 文档解析 | PDFium + office2md (Python) |

---

## 📄 许可证

本项目基于 MIT 许可证开源。详见 [LICENSE](LICENSE) 文件。

---

<div align="center">

**Made with ❤️** · 如有定制需求，请加微信：`AceTranslatePro`

</div>
