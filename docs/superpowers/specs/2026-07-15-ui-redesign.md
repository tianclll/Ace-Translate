# AceTranslatePro UI Redesign — 扁平现代风格

日期: 2026-07-15
状态: 设计已批准

## 概述

为 AceTranslatePro 翻译工具重新设计 UI 界面，从默认的 Qt 原生外观改造为**浅色扁平现代风格**，采用**左侧导航 + 右侧内容区**布局，覆盖 5 大模块。

## 配色方案

| 用途 | 色值 |
|---|---|
| 窗口背景 | `#F5F5F7`（浅灰白） |
| 卡片/面板背景 | `#FFFFFF`（纯白） |
| 导航栏背景 | `#FAFAFA` 微灰 |
| 主色/强调色 | `#007AFF`（苹果蓝） |
| 主色 hover | `#0056CC` |
| 主色 pressed | `#004099` |
| 危险/删除 | `#FF3B30` |
| 成功/完成 | `#34C759` |
| 文本主色 | `#1D1D1F` |
| 文本次要 | `#86868B` |
| 分割线 | `#E5E5EA` |
| 输入框边框 | `#D1D1D6` |
| 输入框 focus | `#007AFF` |
| 导航栏选中 | `#007AFF` 淡底 + icon 蓝色 |
| hover 高亮 | `#E8F0FE` |

## 圆角 & 尺寸

| 元素 | 值 |
|---|---|
| 卡片圆角 | `8px` |
| 按钮圆角 | `6px` |
| 输入框圆角 | `6px` |
| 导航图标按钮 | `44×44 px` |
| 导航栏宽度 | `72px` |
| 翻译按钮最小宽度 | `120px` |
| 图片预览区域 | `min(360, width/2 - 40) × 260` |

## 字体

- 界面：`"Microsoft YaHei UI", "Segoe UI", "PingFang SC", sans-serif`
- 代码/日志区域：`"Cascadia Code", "Consolas", monospace`
- 正文大小：`13px`，输入文本：`14px`，标题：`15px`

## 整体布局

```
┌──────────────────────────────────────────────┐
│ ┌────────┐  ┌───────────────────────────────┐│
│ │        │  │                               ││
│ │ 📝     │  │   内容区域                     ││
│ │ ✂️     │  │   QStackedWidget              ││
│ │ 📸     │  │   5 个面板页                   ││
│ │ 🖼️     │  │                               ││
│ │ 📄     │  │                               ││
│ │        │  │                               ││
│ └────────┘  └───────────────────────────────┘│
│ 状态栏 (底部, 高度 28px)                       │
└──────────────────────────────────────────────┘
```

- **左侧导航栏**: 固定宽度 72px，5 个图标按钮纵向排列，选中态高亮
- **右侧内容区**: QStackedWidget 切换，统一上下左右边距 24px
- **状态栏**: 仅显示进度信息，背景 `#F5F5F7`

## 模块明细

### 1. 文本翻译

```
原文输入
┌──────────────────────────────────────────────┐
│                                              │
│  多行 QPlainTextEdit                         │
│                                              │
│  最小高度 150px                              │
└──────────────────────────────────────────────┘
语言 [中文▼]  Max Tokens [512]            [翻 译]
译文输出 (readonly)
┌──────────────────────────────────────────────┐
│                                              │
│  多行 QPlainTextEdit                         │
│                                              │
└──────────────────────────────────────────────┘
```

- 输入框 placeholder: "在此输入要翻译的文本…"
- 控制栏：左对齐语言选择，中间留白，右对齐 Max Tokens + 按钮

### 2. 划词翻译

页面内显示配置项 + 历史记录。

```
┌───── 划词翻译悬浮窗配置 ───────────────────────┐
│                                                │
│  快捷键：Ctrl+Shift+C         [修改快捷键]       │
│  默认语言：[中文▼]                              │
│  自动复制译文：☑                               │
│  窗口置顶：☑                                   │
│                                                │
│  ──── 翻译历史 ────                            │
│  Hello World          → 你好世界               │
│  Good morning         → 早上好                 │
│  清空历史                                       │
└────────────────────────────────────────────────┘
```

**独立悬浮窗设计**（`Flags: Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint`）：

```
┌──────────────────────────────────────┐
│ ✂️ 翻译  [中文▼]  — □ ✕            │ ← 标题区（可拖动）
├──────────────────────────────────────┤
│ 原文: Hello World                     │ ← 自动取词显示
│ ──────────────────────────            │
│ 译文: 你好世界                         │
│                                       │
│           [锁定] [翻译] [复制]         │ ← 操作按钮
└──────────────────────────────────────┘
```

- 默认大小 360×200px，右下角可缩放
- 选中文本后按 `Ctrl+Shift+C` → 弹出悬浮窗显示翻译
- 关闭后再次触发快捷键重新显示
- 支持锁定/置顶模式

### 3. 截图翻译

```
快捷键: Ctrl+Shift+Z                    [截图翻译]
┌────────────────────┐ ┌────────────────────┐
│                    │ │                    │
│    截图预览        │ │   翻译结果          │
│    QLabel          │ │   QTextEdit        │
│    + QScrollArea   │ │   readonly         │
│                    │ │                    │
└────────────────────┘ └────────────────────┘
语言 [中文▼]  Max Tokens [512]
```

交互流程：
1. 点击「截图翻译」按钮或按 `Ctrl+Shift+Z`
2. 全屏半透明遮罩 + 鼠标变成十字准星
3. 拖拽选区 → 松开完成截图
4. 左侧显示截图缩略预览，右侧开始 OCR 识别 + 翻译
5. 翻译结果显示在右侧文本框

实现方式：使用 QScreen::grabWindow() 捕获全屏，在遮罩上跟踪鼠标绘制选区矩形。

### 4. 图片翻译

```
输入: [___________________________] [选择图片]
┌────────────────────┐ ┌────────────────────┐
│    原图预览         │ │   翻译后图片        │
│    QLabel          │ │   QLabel           │
│    + QScrollArea   │ │   + QScrollArea    │
│                    │ │                    │
└────────────────────┘ └────────────────────┘
输出: [___________________________] [浏览]
语言 [中文▼]  Max Tokens [512]     [翻译图片]
```

- 左右对比布局，翻译完成后右侧自动显示结果图片
- 输出路径默认自动生成（同目录，文件名 `源文件名_trans.png`）

### 5. 文件翻译

```
文件: [___________________________] [浏览]
输出: [___________________________] [浏览]

┌─── 参数 ─────────────────────────────────────┐
│  目标语言: [中文▼]                            │
│  版面阈值: [0.50 ▲▼] (0 ~ 1)                │
│  PDF DPI:  [200 ▲▼] (72 ~ 600)              │
│  ☑ 启用去扭曲  ☐ 启用增强                     │
└──────────────────────────────────────────────┘

                          [开始翻译]

结果:
┌──────────────────────────────────────────────┐
│  翻译完成！输出到：/path/to/output.md          │
└──────────────────────────────────────────────┘
```

- 文件选择支持格式：图片 / PDF / Office (docx, xlsx, pptx) / Markdown / 纯文本

## 全局样式 (QSS)

```qss
/* 窗口 */
QMainWindow { background: #F5F5F7; }

/* 导航栏 */
QFrame#navPanel {
    background: #FAFAFA;
    border-right: 1px solid #E5E5EA;
}
QPushButton#navBtn {
    background: transparent; border: none; border-radius: 8px;
    min-width: 44px; min-height: 44px; font-size: 22px;
}
QPushButton#navBtn:hover { background: #E8F0FE; }
QPushButton#navBtn:checked { background: #007AFF; color: white; }

/* 卡片容器 */
QFrame#card {
    background: white; border: 1px solid #E5E5EA;
    border-radius: 8px; padding: 16px;
}

/* 输入框 */
QLineEdit, QPlainTextEdit, QTextEdit {
    border: 1px solid #D1D1D6; border-radius: 6px;
    padding: 8px 12px; background: white; font-size: 13px;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border-color: #007AFF;
}

/* 主按钮 */
QPushButton#primaryBtn {
    background: #007AFF; color: white; border: none;
    border-radius: 6px; padding: 8px 24px; font-size: 13px; font-weight: bold;
}
QPushButton#primaryBtn:hover { background: #0056CC; }
QPushButton#primaryBtn:pressed { background: #004099; }
QPushButton#primaryBtn:disabled { background: #B0B0B0; }

/* 次要按钮 */
QPushButton#secondaryBtn {
    background: transparent; color: #007AFF;
    border: 1px solid #007AFF; border-radius: 6px;
    padding: 6px 16px; font-size: 13px;
}
QPushButton#secondaryBtn:hover { background: #E8F0FE; }

/* 下拉框 */
QComboBox {
    border: 1px solid #D1D1D6; border-radius: 6px;
    padding: 6px 12px; background: white; min-width: 100px;
}
QComboBox:focus { border-color: #007AFF; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: none; }
QComboBox QAbstractItemView {
    border: 1px solid #D1D1D6; border-radius: 6px;
    padding: 4px; selection-background-color: #E8F0FE;
    selection-color: #1D1D1F;
}

/* SpinBox */
QSpinBox, QDoubleSpinBox {
    border: 1px solid #D1D1D6; border-radius: 6px;
    padding: 6px; background: white;
}

/* 复选框 */
QCheckBox { spacing: 6px; }
QCheckBox::indicator { width: 18px; height: 18px; border-radius: 3px; }
QCheckBox::indicator:checked { background: #007AFF; }

/* 状态栏 */
QStatusBar { background: #F5F5F7; border-top: 1px solid #E5E5EA; font-size: 12px; }

/* 标签页按钮组 */
QPushButton#tabBtn {
    background: transparent; border: none; border-bottom: 2px solid transparent;
    padding: 8px 16px; font-size: 13px; color: #86868B;
}
QPushButton#tabBtn:checked { color: #007AFF; border-bottom: 2px solid #007AFF; }
QPushButton#tabBtn:hover { color: #1D1D1F; }

/* 进度条 */
QProgressBar {
    border: none; border-radius: 3px; background: #E5E5EA; height: 6px;
}
QProgressBar::chunk { background: #007AFF; border-radius: 3px; }

/* 分割线 */
QFrame#separator {
    background: #E5E5EA; max-height: 1px;
}
```

## 文件变更清单

| 文件 | 操作 | 说明 |
|---|---|---|
| `CMakeLists.txt` | 修改 | 添加 `set(CMAKE_AUTORCC ON)`，无新模块依赖 |
| `ui/main.cpp` | 修改 | 加载全局 QSS 样式文件 |
| `ui/mainwindow.h` | 重写 | 新增导航栏控件，新增划词相关成员 |
| `ui/mainwindow.cpp` | 重写 | 新的布局代码 + 截图选区 + 划词逻辑 |
| `ui/resources.qrc` | 新增 | Qt 资源文件，嵌入 QSS 和图标 |
| `ui/style.qss` | 新增 | 全局样式表 |
| `ui/regioncapture.cpp` | 新增 | 截图选区工具窗口 |
| `ui/floatwindow.h` | 新增 | 划词翻译悬浮窗 |
| `ui/floatwindow.cpp` | 新增 | 悬浮窗实现 |

## 截图选区实现方案

关键类 `RegionCapture`（QWidget）：

1. 启动：隐藏主窗口，创建全屏半透明遮罩 `#80000000`
2. 鼠标按下：记录起点
3. 鼠标移动：绘制虚线矩形选区
4. 鼠标松开：捕获选区内的像素 → 传递给 OCR+翻译流程
5. 按 `Esc` 取消，恢复主窗口

```cpp
class RegionCapture : public QWidget {
    Q_OBJECT
    // 全屏遮罩，鼠标事件跟踪
    // 使用 QScreen::grabWindow(0).copy(rect) 获取选区
};
```

## 划词翻译悬浮窗实现

`FloatTranslateWindow`（QWidget）:

- 窗口标志: `Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint`
- 注册全局快捷键: `Ctrl+Shift+C`（通过 `RegisterHotKey` Win32 API）
- 收到快捷键 → `OpenClipboard` 读取文本 → `translate_text()` → 显示结果
- 拖拽移动：重写 `mousePressEvent`/`mouseMoveEvent`
- 缩放：右下角拖拽手柄

## 验证方法

1. 编译成功后启动应用，确认窗口正常显示
2. 检查左侧 5 个导航按钮切换不同模块
3. 文本翻译：输入文字 → 点击翻译 → 显示结果
4. 截图翻译：点击「截图翻译」→ 选区截图 → 显示识别结果
5. 图片翻译：选择图片 → 翻译 → 右侧显示输出图
6. 文件翻译：选择文件 → 翻译 → 显示输出路径
7. 划词翻译：按 Ctrl+Shift+C → 弹出悬浮窗 → 显示翻译
8. 检查深色/浅色高亮状态、hover 效果
