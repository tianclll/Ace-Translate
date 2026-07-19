# AceTranslatePro UI Redesign — 扁平现代风格 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Transform the Qt UI from native tab-widget layout to a modern flat-design left-nav + content area, add floating translate window and screenshot region capture.

**Architecture:** Keep the same C++ Qt6 Widgets backend and `TranslateWorker` pattern. Replace `QTabWidget` with `QPushButton` nav panel + `QStackedWidget`. Add `RegionCapture` for interactive screenshot selection and `FloatTranslateWindow` for global hotkey text translation. All styling via a single `style.qss` resource file.

**Tech Stack:** Qt6.5.2 Widgets, C++17, no new Qt modules

## Global Constraints

- Window background: `#F5F5F7`, nav background: `#FAFAFA`, primary color: `#007AFF`
- Navigation bar fixed width: `72px`, nav icon buttons `44×44px`
- All card containers: white background, `8px` radius, `1px solid #E5E5EA` border
- All input fields: `6px` radius, `1px solid #D1D1D6`, focus border `#007AFF`
- Primary buttons: `#007AFF` background, white text, `6px` radius, `bold`
- All text in `13px` UI font
- All new source files must be added to `CMakeLists.txt` SOURCES list
- Use `CMAKE_AUTORCC ON` for `.qrc` resource compilation
- Screenshot region capture: `RegisterHotKey` Win32 for global shortcut; `QScreen::grabWindow(0)` for fullscreen capture
- FloatTranslateWindow: `Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint`

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `ui/main.cpp` | Modify | Load QSS from resources, set app font |
| `ui/mainwindow.h` | Rewrite | Declare MainWindow with nav + QStackedWidget + 5 panels + TranslateWorker |
| `ui/mainwindow.cpp` | Rewrite | Implement all panel setups, translation slots, worker thread management |
| `ui/regioncapture.h` | Create | `RegionCapture : QWidget` — fullscreen overlay for screenshot selection |
| `ui/regioncapture.cpp` | Create | Mouse tracking, rubber band rect, Esc cancel, grab captured pixels |
| `ui/floatwindow.h` | Create | `FloatTranslateWindow : QWidget` — floating tool window |
| `ui/floatwindow.cpp` | Create | Title bar drag, content area, global hotkey listener via RegisterHotKey |
| `ui/style.qss` | Create | Global Qt stylesheet — all colors, radii, hover/pressed/focus states |
| `ui/resources.qrc` | Create | Qt resource file — embeds style.qss |
| `CMakeLists.txt` | Modify | Add new sources, enable AUTORCC |
| `docs/superpowers/plans/2026-07-15-ui-redesign-plan.md` | Create | This file |

---

### Task 1: QSS Stylesheet + Resources + CMake

**Files:**
- Create: `ui/style.qss`
- Create: `ui/resources.qrc`
- Modify: `CMakeLists.txt`
- Modify: `ui/main.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: `:/style/style.qss` resource path consumed by main.cpp

- [ ] **Step 1: Create `ui/style.qss`**

```qss
/* ============================================================
   AceTranslatePro — 扁平现代风格样式表
   ============================================================ */

/* 窗口 */
QMainWindow { background: #F5F5F7; }

/* 导航栏面板 */
QFrame#navPanel {
    background: #FAFAFA;
    border-right: 1px solid #E5E5EA;
}

/* 导航按钮 */
QPushButton#navBtn {
    background: transparent; border: none; border-radius: 8px;
    min-width: 44px; min-height: 44px; font-size: 22px;
}
QPushButton#navBtn:hover { background: #E8F0FE; }
QPushButton#navBtn:checked { background: #007AFF; color: white; }

/* 导航标签 */
QLabel#navLabel {
    font-size: 10px; color: #86868B; border: none;
}
QPushButton#navBtn:checked + QLabel#navLabel { color: #007AFF; }

/* 卡片容器 */
QFrame#card {
    background: white; border: 1px solid #E5E5EA;
    border-radius: 8px; padding: 16px;
}

/* 输入框 */
QLineEdit, QPlainTextEdit, QTextEdit {
    border: 1px solid #D1D1D6; border-radius: 6px;
    padding: 8px 12px; background: white; font-size: 13px; color: #1D1D1F;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border-color: #007AFF;
}
QLineEdit:disabled, QPlainTextEdit:disabled, QTextEdit:disabled {
    background: #F5F5F7; color: #86868B;
}

/* 主按钮 */
QPushButton#primaryBtn {
    background: #007AFF; color: white; border: none;
    border-radius: 6px; padding: 8px 24px; font-size: 13px; font-weight: bold;
    min-width: 100px;
}
QPushButton#primaryBtn:hover { background: #0056CC; }
QPushButton#primaryBtn:pressed { background: #004099; }
QPushButton#primaryBtn:disabled { background: #B0B0B0; color: white; }

/* 次要按钮 */
QPushButton#secondaryBtn {
    background: transparent; color: #007AFF;
    border: 1px solid #007AFF; border-radius: 6px;
    padding: 6px 16px; font-size: 13px;
}
QPushButton#secondaryBtn:hover { background: #E8F0FE; }
QPushButton#secondaryBtn:pressed { background: #D0E5FF; }
QPushButton#secondaryBtn:disabled { color: #B0B0B0; border-color: #B0B0B0; }

/* 下拉框 */
QComboBox {
    border: 1px solid #D1D1D6; border-radius: 6px;
    padding: 6px 12px; background: white; min-width: 100px;
    color: #1D1D1F; font-size: 13px;
}
QComboBox:focus { border-color: #007AFF; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { width: 0; height: 0; border-left: 5px solid transparent;
    border-right: 5px solid transparent; border-top: 6px solid #86868B; margin-right: 6px; }
QComboBox QAbstractItemView {
    border: 1px solid #D1D1D6; border-radius: 6px;
    padding: 4px; selection-background-color: #E8F0FE;
    selection-color: #1D1D1F; outline: none;
}

/* SpinBox */
QSpinBox, QDoubleSpinBox {
    border: 1px solid #D1D1D6; border-radius: 6px;
    padding: 6px; background: white; color: #1D1D1F; font-size: 13px;
}
QSpinBox:focus, QDoubleSpinBox:focus { border-color: #007AFF; }
QSpinBox::up-button, QDoubleSpinBox::up-button {
    border: none; background: transparent; width: 20px;
    subcontrol-position: top right; subcontrol-origin: content;
}
QSpinBox::down-button, QDoubleSpinBox::down-button {
    border: none; background: transparent; width: 20px;
    subcontrol-position: bottom right; subcontrol-origin: content;
}

/* 复选框 */
QCheckBox { spacing: 8px; font-size: 13px; color: #1D1D1F; }
QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px;
    border: 1.5px solid #D1D1D6; background: white; }
QCheckBox::indicator:hover { border-color: #007AFF; }
QCheckBox::indicator:checked { background: #007AFF; border-color: #007AFF; }

/* 状态栏 */
QStatusBar { background: #F5F5F7; border-top: 1px solid #E5E5EA; font-size: 12px; color: #86868B; }

/* 进度条 */
QProgressBar {
    border: none; border-radius: 3px; background: #E5E5EA; height: 6px; text-align: center; font-size: 10px;
}
QProgressBar::chunk { background: #007AFF; border-radius: 3px; }

/* 标签 */
QLabel#sectionTitle { font-size: 15px; font-weight: bold; color: #1D1D1F; }
QLabel#sectionHint { font-size: 12px; color: #86868B; }

/* 分割线 */
QFrame#separator {
    background: #E5E5EA; max-height: 1px;
}

/* GroupBox — 无边框，保留标题 */
QGroupBox {
    font-size: 13px; font-weight: bold; color: #1D1D1F;
    border: none; margin-top: 8px; padding-top: 12px;
}
QGroupBox::title { subcontrol-origin: margin; left: 0px; padding: 0px; }

/* ScrollArea */
QScrollArea { border: 1px solid #E5E5EA; border-radius: 6px; background: white; }

/* 文字选中 */
QPlainTextEdit, QTextEdit, QLineEdit { selection-background-color: #007AFF; selection-color: white; }
```

- [ ] **Step 2: Create `ui/resources.qrc`**

```xml
<RCC>
    <qresource prefix="/style">
        <file>style.qss</file>
    </qresource>
</RCC>
```

- [ ] **Step 3: Modify `ui/main.cpp`** — add QSS loading and font setup

```cpp
#include <QApplication>
#include <QFile>
#include <QFont>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("AceTranslatePro");
    app.setApplicationVersion("1.0.0");

    // 加载全局样式表
    QFile styleFile(":/style/style.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    // 设置默认字体
    QFont defaultFont("Microsoft YaHei UI", 10);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(defaultFont);

    MainWindow w;
    w.show();

    return app.exec();
}
```

- [ ] **Step 4: Modify `CMakeLists.txt`** — add AUTORCC and new sources

Replace `set(CMAKE_AUTOMOC ON)` with:
```cmake
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
```

In the SOURCES list, add after `ui/mainwindow.cpp`:
```cmake
        ui/regioncapture.cpp
        ui/floatwindow.cpp
        ui/resources.qrc
```

Also add `ui` to include_directories so `mainwindow.h` can include the sibling files:
```cmake
include_directories(ui)
```
(Already covered by `include_directories(include)` — if needed, just add ui to it.)

Actually the existing `include_directories(include)` won't help since `mainwindow.h` is in `ui/`. The `#include "mainwindow.h"` in main.cpp works because they're in the same directory. For regioncapture.h and floatwindow.h, we need either relative includes or add ui to include dirs. Since these are included from mainwindow.cpp (same dir), just use `#include "regioncapture.h"`.

- [ ] **Step 5: Verify build compiles**

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build .
```
Expected: compiles (window won't show properly until later tasks fill in content)

---

### Task 2: MainWindow Rewrite — Navigation + Stacked Widget Shell

**Files:**
- Rewrite: `ui/mainwindow.h`
- Rewrite: `ui/mainwindow.cpp` (layout + nav only, slot implementations come in Task 3-7)

**Interfaces:**
- Consumes: `translate_text()`, `process_file()`, `process_photo()`, `translate_screenshot_image()` from `docmind/DocumentEngine.h`
- Consumes: `RegionCapture` (declared in regioncapture.h, implemented in Task 8)
- Consumes: `FloatTranslateWindow` (declared in floatwindow.h, implemented in Task 9)
- Produces: `MainWindow` class with 5 stacked pages, nav switching, TranslateWorker

Hard requirement: the worker and threading logic from the current implementation must be preserved **exactly**, only the slot bodies and layout change.

- [ ] **Step 1: Write `ui/mainwindow.h`**

```cpp
#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QFrame>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QStatusBar>
#include <QThread>
#include <QAtomicInt>
#include <QVector>

// ============================================================
// 工作线程（避免 UI 卡死）
// ============================================================
class TranslateWorker : public QObject {
    Q_OBJECT
public:
    enum Mode {
        TextTranslate,
        FileTranslate,
        ScreenshotTranslate,
        PhotoTranslate
    };

    Mode mode;
    QString inputText;
    QString inputPath;
    QString outputPath;
    QString baseDir;
    QString targetLang;
    QString currentFilePath;
    float layoutThreshold = 0.5f;
    int pdfDpi = 200;
    int maxTokens = 512;
    bool enableWarp = true;
    bool enableEnhance = false;

    // 截图模式用 — 直接传入 cv::Mat
    cv::Mat screenshotMat;

public slots:
    void run();

signals:
    void progressUpdated(const QString& msg);
    void finished(const QString& result);
    void errorOccurred(const QString& err);
};

// 前向声明
class RegionCapture;
class FloatTranslateWindow;

// ============================================================
// 主窗口
// ============================================================
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // 导航
    void onNavButtonClicked(int index);

    // 文本翻译
    void onTranslateText();

    // 划词翻译
    void onFloatWindowConfigChanged();

    // 截图翻译
    void onCaptureScreenshot();
    void onTranslateScreenshotLocal();

    // 图片翻译
    void onSelectPhotoInput();
    void onSelectPhotoOutput();
    void onProcessPhoto();

    // 文件翻译
    void onSelectInputFile();
    void onSelectOutputDir();
    void onProcessFile();

    // 设置
    void onBrowseBaseDir();

    // 工作线程回调
    void onWorkerFinished(const QString& result);
    void onWorkerError(const QString& err);
    void onWorkerProgress(const QString& msg);

private:
    void setupUI();
    QWidget* createNavButton(const QString& icon, const QString& tooltip, int index);

    // 各面板
    QWidget* createTextPanel();
    QWidget* createFloatConfigPanel();
    QWidget* createScreenshotPanel();
    QWidget* createPhotoPanel();
    QWidget* createFilePanel();
    QWidget* createSettingsPanel();

    void runWorker(TranslateWorker* worker);
    QWidget* wrapInCard(QWidget* content, const QString& title = "");

    // -- 通用 --
    QStackedWidget* stackedWidget_;
    QStatusBar* statusBar_;
    QProgressBar* progressBar_;
    QThread* workerThread_ = nullptr;
    QAtomicInt busy_ = 0;
    QVector<QPushButton*> navButtons_;
    int currentNavIndex_ = 0;

    // -- 子窗口 --
    RegionCapture* regionCapture_ = nullptr;
    FloatTranslateWindow* floatWindow_ = nullptr;

    // -- 文本翻译 --
    QPlainTextEdit* textInput_;
    QPlainTextEdit* textOutput_;
    QComboBox* textLangCombo_;
    QSpinBox* textMaxTokens_;
    QPushButton* textTranslateBtn_;

    // -- 划词翻译 --
    QLineEdit* floatHotkeyEdit_;
    QComboBox* floatLangCombo_;
    QCheckBox* floatAutoCopy_;
    QCheckBox* floatStayOnTop_;
    QTextEdit* floatHistoryView_;

    // -- 截图翻译 --
    QLabel* screenshotPreview_;
    QComboBox* screenshotLangCombo_;
    QSpinBox* screenshotMaxTokens_;
    QTextEdit* screenshotResult_;
    QPushButton* screenshotCaptureBtn_;
    cv::Mat lastCapturedMat_;

    // -- 图片翻译 --
    QLineEdit* photoInputPath_;
    QLineEdit* photoOutputPath_;
    QLabel* photoInputPreview_;
    QLabel* photoOutputPreview_;
    QComboBox* photoLangCombo_;
    QSpinBox* photoMaxTokens_;
    QPushButton* photoTranslateBtn_;

    // -- 文件翻译 --
    QLineEdit* fileInputPath_;
    QLineEdit* fileOutputDir_;
    QComboBox* fileLangCombo_;
    QDoubleSpinBox* fileLayoutThreshold_;
    QSpinBox* filePdfDpi_;
    QCheckBox* fileEnableWarp_;
    QCheckBox* fileEnableEnhance_;
    QTextEdit* fileResultView_;
    QPushButton* fileProcessBtn_;

    // -- 设置 --
    QLineEdit* baseDirPath_;
    QLabel* configStatusLabel_;
};
```

- [ ] **Step 2: Write `TranslateWorker::run()` in `mainwindow.cpp`** (same logic as current, add screenshotMat support)

The run() method is IDENTICAL to the current implementation except:
- ScreenshotTranslate mode reads from `screenshotMat` instead of loading from disk.
- Text mode is unchanged.
- File mode is unchanged.
- Photo mode is unchanged.

```cpp
#include "mainwindow.h"
#include "docmind/DocumentEngine.h"
#include "regioncapture.h"
#include "floatwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QPixmap>
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <opencv2/opencv.hpp>

// ============================================================
// TranslateWorker
// ============================================================
void TranslateWorker::run() {
    try {
        switch (mode) {
        case TextTranslate: {
            emit progressUpdated(QStringLiteral("翻译中…"));
            std::string result = translate_text(
                inputText.toStdString(),
                targetLang.toStdString(),
                maxTokens);
            emit finished(QString::fromStdString(result));
            break;
        }
        case FileTranslate: {
            emit progressUpdated(QStringLiteral("处理文件: ") + currentFilePath);
            std::string out = process_file(
                inputPath.toStdString(),
                outputPath.toStdString(),
                baseDir.toStdString(),
                targetLang.toStdString(),
                layoutThreshold, pdfDpi,
                enableWarp, enableEnhance);
            emit finished(QString::fromStdString(out));
            break;
        }
        case ScreenshotTranslate: {
            emit progressUpdated(QStringLiteral("OCR 识别并翻译截图…"));
            if (screenshotMat.empty()) {
                emit errorOccurred(QStringLiteral("截图内容为空"));
                return;
            }
            std::string result = translate_screenshot_image(screenshotMat,
                targetLang.toStdString(), maxTokens);
            emit finished(QString::fromStdString(result));
            break;
        }
        case PhotoTranslate: {
            emit progressUpdated(QStringLiteral("翻译图片…"));
            std::string out = process_photo(
                inputPath.toStdString(),
                outputPath.toStdString(),
                baseDir.toStdString(),
                targetLang.toStdString(),
                maxTokens);
            emit finished(QString::fromStdString(out));
            break;
        }
        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    } catch (...) {
        emit errorOccurred(QStringLiteral("未知错误"));
    }
}
```

- [ ] **Step 3: Write `MainWindow` constructor + `setupUI()` + nav buttons**

```cpp
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    regionCapture_ = new RegionCapture(this);
    floatWindow_ = new FloatTranslateWindow(nullptr); // top-level, no parent

    // 连接截图完成信号
    connect(regionCapture_, &RegionCapture::regionCaptured,
            this, &MainWindow::onTranslateScreenshotLocal);

    setupUI();
    resize(1000, 720);

    // 默认选中第一个导航项
    onNavButtonClicked(0);
}

MainWindow::~MainWindow() {
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(3000);
    }
    delete floatWindow_; // top-level, manual cleanup
}

void MainWindow::setupUI() {
    setWindowTitle(QStringLiteral("AceTranslatePro - 翻译工具"));

    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- 左侧导航栏 ----
    auto* navPanel = new QFrame;
    navPanel->setObjectName("navPanel");
    navPanel->setFixedWidth(72);
    auto* navLayout = new QVBoxLayout(navPanel);
    navLayout->setContentsMargins(8, 16, 8, 16);
    navLayout->setSpacing(4);

    // Logo/标题区
    auto* logoLabel = new QLabel(QStringLiteral("AT"));
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#007AFF; padding:8px 0;");
    navLayout->addWidget(logoLabel);

    // 分割线
    auto* sep1 = new QFrame;
    sep1->setObjectName("separator");
    sep1->setFixedHeight(1);
    navLayout->addWidget(sep1);
    navLayout->addSpacing(8);

    navLayout->addWidget(createNavButton(QStringLiteral("📝"), QStringLiteral("文本翻译"), 0));
    navLayout->addWidget(createNavButton(QStringLiteral("✂️"), QStringLiteral("划词翻译"), 1));
    navLayout->addWidget(createNavButton(QStringLiteral("📸"), QStringLiteral("截图翻译"), 2));
    navLayout->addWidget(createNavButton(QStringLiteral("🖼️"), QStringLiteral("图片翻译"), 3));
    navLayout->addWidget(createNavButton(QStringLiteral("📄"), QStringLiteral("文件翻译"), 4));

    navLayout->addStretch();

    // 设置按钮在底部
    navLayout->addSpacing(8);
    auto* sep2 = new QFrame;
    sep2->setObjectName("separator");
    sep2->setFixedHeight(1);
    navLayout->addWidget(sep2);
    navLayout->addSpacing(8);
    navLayout->addWidget(createNavButton(QStringLiteral("⚙"), QStringLiteral("设置"), 5));

    mainLayout->addWidget(navPanel);

    // ---- 右侧内容区 ----
    stackedWidget_ = new QStackedWidget;
    stackedWidget_->addWidget(createTextPanel());       // 0
    stackedWidget_->addWidget(createFloatConfigPanel()); // 1
    stackedWidget_->addWidget(createScreenshotPanel());   // 2
    stackedWidget_->addWidget(createPhotoPanel());        // 3
    stackedWidget_->addWidget(createFilePanel());         // 4
    stackedWidget_->addWidget(createSettingsPanel());     // 5

    // 内容区外包装边距
    auto* contentFrame = new QFrame;
    contentFrame->setObjectName("card");
    auto* contentLayout = new QVBoxLayout(contentFrame);
    contentLayout->setContentsMargins(24, 24, 24, 24);
    contentLayout->addWidget(stackedWidget_);
    mainLayout->addWidget(contentFrame, 1);

    setCentralWidget(centralWidget);

    // ---- 状态栏 ----
    statusBar_ = statusBar();
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->setFixedWidth(180);
    progressBar_->hide();
    statusBar_->addPermanentWidget(progressBar_);
    statusBar_->showMessage(QStringLiteral("就绪"));
}

QWidget* MainWindow::createNavButton(const QString& icon, const QString& tooltip, int index) {
    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* btn = new QPushButton(icon);
    btn->setObjectName("navBtn");
    btn->setToolTip(tooltip);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    connect(btn, &QPushButton::clicked, this, [this, index]() {
        onNavButtonClicked(index);
    });

    auto* label = new QLabel(tooltip);
    label->setObjectName("navLabel");
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(btn, 0, Qt::AlignCenter);
    layout->addWidget(label, 0, Qt::AlignCenter);

    navButtons_.append(btn);
    return container;
}

void MainWindow::onNavButtonClicked(int index) {
    if (index < 0 || index >= navButtons_.size()) return;
    currentNavIndex_ = index;

    for (int i = 0; i < navButtons_.size(); ++i) {
        navButtons_[i]->setChecked(i == index);
    }
    stackedWidget_->setCurrentIndex(index);
    statusBar_->showMessage(QStringLiteral("就绪"));
}
```

- [ ] **Step 4: Verify build**

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build .
```
Expected: compiles. Run shows left nav with 6 buttons, clicking switches content panels (empty white pages).

---

### Task 3: Text Translate Panel

**Files:**
- Modify: `ui/mainwindow.cpp` — implement `createTextPanel()` + `onTranslateText()` + `runWorker()` + `onWorkerFinished/Error/Progress`

No new files. This is a port of the existing text tab.

- [ ] **Step 1: Write `createTextPanel()` in mainwindow.cpp**

```cpp
QWidget* MainWindow::createTextPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("文本翻译"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("输入要翻译的文本，选择目标语言后点击翻译"));
    hint->setObjectName("sectionHint");
    layout->addWidget(hint);

    // 原文输入
    textInput_ = new QPlainTextEdit;
    textInput_->setPlaceholderText(QStringLiteral("在此输入要翻译的文本…"));
    textInput_->setMinimumHeight(140);
    layout->addWidget(textInput_);

    // 控制栏
    auto* ctrlLayout = new QHBoxLayout;
    ctrlLayout->setSpacing(8);
    ctrlLayout->addWidget(new QLabel(QStringLiteral("目标语言:")));
    textLangCombo_ = new QComboBox;
    textLangCombo_->setEditable(true);
    textLangCombo_->addItems({
        QStringLiteral("中文"), QStringLiteral("English"),
        QStringLiteral("日本語"), QStringLiteral("한국어"),
        QStringLiteral("Français"), QStringLiteral("Deutsch"),
        QStringLiteral("Español"), QStringLiteral("Русский"),
        QStringLiteral("繁體中文")
    });
    textLangCombo_->setCurrentText(QStringLiteral("中文"));
    ctrlLayout->addWidget(textLangCombo_);

    ctrlLayout->addStretch();
    ctrlLayout->addWidget(new QLabel(QStringLiteral("Max Tokens:")));
    textMaxTokens_ = new QSpinBox;
    textMaxTokens_->setRange(64, 4096);
    textMaxTokens_->setValue(512);
    ctrlLayout->addWidget(textMaxTokens_);

    textTranslateBtn_ = new QPushButton(QStringLiteral("翻译"));
    textTranslateBtn_->setObjectName("primaryBtn");
    connect(textTranslateBtn_, &QPushButton::clicked, this, &MainWindow::onTranslateText);
    ctrlLayout->addWidget(textTranslateBtn_);
    layout->addLayout(ctrlLayout);

    // 译文输出
    auto* outTitle = new QLabel(QStringLiteral("翻译结果"));
    outTitle->setObjectName("sectionTitle");
    layout->addWidget(outTitle);

    textOutput_ = new QPlainTextEdit;
    textOutput_->setReadOnly(true);
    textOutput_->setPlaceholderText(QStringLiteral("翻译结果将显示在这里…"));
    textOutput_->setMinimumHeight(140);
    layout->addWidget(textOutput_);

    return panel;
}
```

- [ ] **Step 2: Write slot `onTranslateText()` + `runWorker()` + worker callbacks**

```cpp
void MainWindow::onTranslateText() {
    if (busy_.loadRelaxed()) {
        statusBar_->showMessage(QStringLiteral("正在处理中，请等待…"), 3000);
        return;
    }
    QString text = textInput_->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要翻译的文本"));
        return;
    }

    auto* worker = new TranslateWorker;
    worker->mode = TranslateWorker::TextTranslate;
    worker->inputText = text;
    worker->targetLang = textLangCombo_->currentText();
    worker->maxTokens = textMaxTokens_->value();
    runWorker(worker);
}

void MainWindow::runWorker(TranslateWorker* worker) {
    busy_.storeRelaxed(1);

    // 禁用所有操作按钮
    if (textTranslateBtn_) textTranslateBtn_->setEnabled(false);
    if (fileProcessBtn_) fileProcessBtn_->setEnabled(false);
    if (screenshotCaptureBtn_) screenshotCaptureBtn_->setEnabled(false);
    if (photoTranslateBtn_) photoTranslateBtn_->setEnabled(false);

    workerThread_ = new QThread(this);
    worker->moveToThread(workerThread_);

    connect(workerThread_, &QThread::started, worker, &TranslateWorker::run);
    connect(worker, &TranslateWorker::finished, this, &MainWindow::onWorkerFinished);
    connect(worker, &TranslateWorker::errorOccurred, this, &MainWindow::onWorkerError);
    connect(worker, &TranslateWorker::progressUpdated, this, &MainWindow::onWorkerProgress);
    connect(workerThread_, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread_, &QThread::finished, workerThread_, &QObject::deleteLater);

    progressBar_->show();
    statusBar_->showMessage(QStringLiteral("处理中…"));
    workerThread_->start();
}

void MainWindow::onWorkerFinished(const QString& result) {
    progressBar_->hide();
    statusBar_->showMessage(QStringLiteral("完成"), 5000);

    // 根据当前导航索引填入结果
    switch (currentNavIndex_) {
    case 0: // 文本翻译
        textOutput_->setPlainText(result);
        break;
    case 2: // 截图翻译
        screenshotResult_->setPlainText(result);
        break;
    case 4: // 文件翻译
        fileResultView_->setPlainText(result);
        break;
    }

    busy_.storeRelaxed(0);
    if (textTranslateBtn_) textTranslateBtn_->setEnabled(true);
    if (fileProcessBtn_) fileProcessBtn_->setEnabled(true);
    if (screenshotCaptureBtn_) screenshotCaptureBtn_->setEnabled(true);
    if (photoTranslateBtn_) photoTranslateBtn_->setEnabled(true);
}

void MainWindow::onWorkerError(const QString& err) {
    progressBar_->hide();
    statusBar_->showMessage(QStringLiteral("错误"), 5000);
    QMessageBox::warning(this, QStringLiteral("翻译出错"), err);

    busy_.storeRelaxed(0);
    if (textTranslateBtn_) textTranslateBtn_->setEnabled(true);
    if (fileProcessBtn_) fileProcessBtn_->setEnabled(true);
    if (screenshotCaptureBtn_) screenshotCaptureBtn_->setEnabled(true);
    if (photoTranslateBtn_) photoTranslateBtn_->setEnabled(true);
}

void MainWindow::onWorkerProgress(const QString& msg) {
    statusBar_->showMessage(msg);
}
```

---

### Task 4: File Translate Panel

Port existing file tab. No new files.

**Files:**
- Modify: `ui/mainwindow.cpp` — `createFilePanel()`, `onSelectInputFile()`, `onSelectOutputDir()`, `onProcessFile()`

- [ ] **Step 1: Write `createFilePanel()`**

```cpp
QWidget* MainWindow::createFilePanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("文件翻译"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("支持图片、PDF、Office (docx/xlsx/pptx)、Markdown、纯文本文件"));
    hint->setObjectName("sectionHint");
    layout->addWidget(hint);

    // 文件选择
    auto* fileCard = new QFrame;
    fileCard->setObjectName("card");
    auto* fileForm = new QFormLayout(fileCard);

    auto* inputRow = new QHBoxLayout;
    fileInputPath_ = new QLineEdit;
    fileInputPath_->setPlaceholderText(QStringLiteral("选择图片 / PDF / Office / Markdown / 文本文件…"));
    inputRow->addWidget(fileInputPath_);
    auto* browseInputBtn = new QPushButton(QStringLiteral("浏览…"));
    browseInputBtn->setObjectName("secondaryBtn");
    connect(browseInputBtn, &QPushButton::clicked, this, &MainWindow::onSelectInputFile);
    inputRow->addWidget(browseInputBtn);
    fileForm->addRow(QStringLiteral("文件:"), inputRow);

    auto* outputRow = new QHBoxLayout;
    fileOutputDir_ = new QLineEdit;
    fileOutputDir_->setPlaceholderText(QStringLiteral("留空则自动生成到同目录"));
    outputRow->addWidget(fileOutputDir_);
    auto* browseOutBtn = new QPushButton(QStringLiteral("浏览…"));
    browseOutBtn->setObjectName("secondaryBtn");
    connect(browseOutBtn, &QPushButton::clicked, this, &MainWindow::onSelectOutputDir);
    outputRow->addWidget(browseOutBtn);
    fileForm->addRow(QStringLiteral("输出目录:"), outputRow);
    layout->addWidget(fileCard);

    // 参数卡片
    auto* paramCard = new QFrame;
    paramCard->setObjectName("card");
    auto* paramForm = new QFormLayout(paramCard);

    fileLangCombo_ = new QComboBox;
    fileLangCombo_->setEditable(true);
    fileLangCombo_->addItems({QStringLiteral("中文"), QStringLiteral("English"),
        QStringLiteral("日本語"), QStringLiteral("한국어")});
    fileLangCombo_->setCurrentText(QStringLiteral("中文"));
    paramForm->addRow(QStringLiteral("目标语言:"), fileLangCombo_);

    fileLayoutThreshold_ = new QDoubleSpinBox;
    fileLayoutThreshold_->setRange(0.0, 1.0);
    fileLayoutThreshold_->setSingleStep(0.05);
    fileLayoutThreshold_->setValue(0.5);
    paramForm->addRow(QStringLiteral("版面阈值:"), fileLayoutThreshold_);

    filePdfDpi_ = new QSpinBox;
    filePdfDpi_->setRange(72, 600);
    filePdfDpi_->setValue(200);
    paramForm->addRow(QStringLiteral("PDF DPI:"), filePdfDpi_);

    fileEnableWarp_ = new QCheckBox(QStringLiteral("启用去扭曲"));
    fileEnableWarp_->setChecked(true);
    paramForm->addRow(QStringLiteral(""), fileEnableWarp_);

    fileEnableEnhance_ = new QCheckBox(QStringLiteral("启用增强"));
    fileEnableEnhance_->setChecked(false);
    paramForm->addRow(QStringLiteral(""), fileEnableEnhance_);
    layout->addWidget(paramCard);

    // 操作按钮
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    fileProcessBtn_ = new QPushButton(QStringLiteral("开始翻译"));
    fileProcessBtn_->setObjectName("primaryBtn");
    connect(fileProcessBtn_, &QPushButton::clicked, this, &MainWindow::onProcessFile);
    btnRow->addWidget(fileProcessBtn_);
    layout->addLayout(btnRow);

    // 结果
    auto* resultTitle = new QLabel(QStringLiteral("处理结果"));
    resultTitle->setObjectName("sectionTitle");
    layout->addWidget(resultTitle);
    fileResultView_ = new QTextEdit;
    fileResultView_->setReadOnly(true);
    fileResultView_->setPlaceholderText(QStringLiteral("翻译结果路径将显示在这里…"));
    fileResultView_->setMaximumHeight(120);
    layout->addWidget(fileResultView_);

    return panel;
}
```

- [ ] **Step 2: Write slots**

```cpp
void MainWindow::onSelectInputFile() {
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择文件"), QString(),
        QStringLiteral("支持的文件 (*.png *.jpg *.jpeg *.bmp *.tiff *.pdf *.docx *.xlsx *.pptx *.md *.txt);;"
                       "图片 (*.png *.jpg *.jpeg *.bmp *.tiff);;PDF (*.pdf);;"
                       "Office (*.docx *.xlsx *.pptx);;Markdown (*.md);;文本 (*.txt);;所有文件 (*)"));
    if (!path.isEmpty()) fileInputPath_->setText(path);
}

void MainWindow::onSelectOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
    if (!dir.isEmpty()) fileOutputDir_->setText(dir);
}

void MainWindow::onProcessFile() {
    if (busy_.loadRelaxed()) {
        statusBar_->showMessage(QStringLiteral("正在处理中，请等待…"), 3000);
        return;
    }
    QString inputPath = fileInputPath_->text().trimmed();
    if (inputPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择输入文件"));
        return;
    }

    auto* worker = new TranslateWorker;
    worker->mode = TranslateWorker::FileTranslate;
    worker->inputPath = inputPath;
    worker->outputPath = fileOutputDir_->text().trimmed();
    worker->targetLang = fileLangCombo_->currentText();
    worker->layoutThreshold = static_cast<float>(fileLayoutThreshold_->value());
    worker->pdfDpi = filePdfDpi_->value();
    worker->enableWarp = fileEnableWarp_->isChecked();
    worker->enableEnhance = fileEnableEnhance_->isChecked();
    worker->currentFilePath = inputPath;
    runWorker(worker);
}
```

- [ ] **Step 3: `onWorkerFinished` case 4 handles file translation** (already covered in the switch above)

---

### Task 5: Screenshot Translate Panel (UI only, RegionCapture in Task 8)

**Files:**
- Modify: `ui/mainwindow.cpp` — `createScreenshotPanel()`

- [ ] **Step 1: Write `createScreenshotPanel()`**

```cpp
QWidget* MainWindow::createScreenshotPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("截图翻译"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("点击「截图翻译」按钮或按 Ctrl+Shift+Z 截取屏幕区域，自动识别并翻译"));
    hint->setObjectName("sectionHint");
    layout->addWidget(hint);

    // 操作按钮 + 快捷键显示
    auto* actionRow = new QHBoxLayout;
    screenshotCaptureBtn_ = new QPushButton(QStringLiteral("📸 截图翻译"));
    screenshotCaptureBtn_->setObjectName("primaryBtn");
    connect(screenshotCaptureBtn_, &QPushButton::clicked, this, &MainWindow::onCaptureScreenshot);
    actionRow->addWidget(screenshotCaptureBtn_);

    auto* shortcutLabel = new QLabel(QStringLiteral("  快捷键: Ctrl+Shift+Z"));
    shortcutLabel->setObjectName("sectionHint");
    actionRow->addWidget(shortcutLabel);

    actionRow->addStretch();
    layout->addLayout(actionRow);

    // 预览 + 结果 左右布局
    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);

    // 左侧 — 截图预览
    auto* previewCard = new QFrame;
    previewCard->setObjectName("card");
    auto* previewInner = new QVBoxLayout(previewCard);
    previewInner->addWidget(new QLabel(QStringLiteral("截图预览")));
    screenshotPreview_ = new QLabel;
    screenshotPreview_->setFixedSize(360, 240);
    screenshotPreview_->setAlignment(Qt::AlignCenter);
    screenshotPreview_->setStyleSheet("background:#F5F5F7; border-radius:4px; color:#86868B;");
    screenshotPreview_->setText(QStringLiteral("等待截图…"));
    auto* previewScroll = new QScrollArea;
    previewScroll->setWidget(screenshotPreview_);
    previewScroll->setFixedHeight(260);
    previewScroll->setWidgetResizable(false);
    previewInner->addWidget(previewScroll);
    contentRow->addWidget(previewCard);

    // 右侧 — 翻译结果
    auto* resultCard = new QFrame;
    resultCard->setObjectName("card");
    auto* resultInner = new QVBoxLayout(resultCard);
    resultInner->addWidget(new QLabel(QStringLiteral("识别结果")));
    screenshotResult_ = new QTextEdit;
    screenshotResult_->setReadOnly(true);
    screenshotResult_->setPlaceholderText(QStringLiteral("截图后识别和翻译结果将显示在这里…"));
    resultInner->addWidget(screenshotResult_);
    contentRow->addWidget(resultCard, 1);
    layout->addLayout(contentRow);

    // 参数行
    auto* ctrlRow = new QHBoxLayout;
    ctrlRow->addWidget(new QLabel(QStringLiteral("目标语言:")));
    screenshotLangCombo_ = new QComboBox;
    screenshotLangCombo_->setEditable(true);
    screenshotLangCombo_->addItems({
        QStringLiteral("中文"), QStringLiteral("English"),
        QStringLiteral("日本語"), QStringLiteral("한국어")
    });
    screenshotLangCombo_->setCurrentText(QStringLiteral("中文"));
    ctrlRow->addWidget(screenshotLangCombo_);
    ctrlRow->addStretch();
    ctrlRow->addWidget(new QLabel(QStringLiteral("Max Tokens:")));
    screenshotMaxTokens_ = new QSpinBox;
    screenshotMaxTokens_->setRange(64, 4096);
    screenshotMaxTokens_->setValue(512);
    ctrlRow->addWidget(screenshotMaxTokens_);
    layout->addLayout(ctrlRow);

    return panel;
}
```

- [ ] **Step 2: Write slot stubs that will be completed when RegionCapture exists**

```cpp
void MainWindow::onCaptureScreenshot() {
    if (busy_.loadRelaxed()) return;
    hide();
    // 截图完成后 RegionCapture 信号触发 onTranslateScreenshotLocal
    regionCapture_->startCapture();
}

void MainWindow::onTranslateScreenshotLocal() {
    // Called when RegionCapture captures a region
    // Gets called AFTER startCapture completes — see Task 8
    show();
    raise();
    activateWindow();

    // Get the captured image
    lastCapturedMat_ = regionCapture_->capturedImage();
    if (lastCapturedMat_.empty()) {
        statusBar_->showMessage(QStringLiteral("截图取消"), 3000);
        return;
    }

    // Show thumbnail preview
    QImage qimg(lastCapturedMat_.data, lastCapturedMat_.cols, lastCapturedMat_.rows,
                static_cast<int>(lastCapturedMat_.step), QImage::Format_BGR888);
    QPixmap pix = QPixmap::fromImage(qimg.rgbSwapped());
    screenshotPreview_->setPixmap(pix.scaled(360, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Start translation
    auto* worker = new TranslateWorker;
    worker->mode = TranslateWorker::ScreenshotTranslate;
    worker->screenshotMat = lastCapturedMat_.clone();
    worker->targetLang = screenshotLangCombo_->currentText();
    worker->maxTokens = screenshotMaxTokens_->value();
    runWorker(worker);
}
```

---

### Task 6: Photo Translate Panel

**Files:**
- Modify: `ui/mainwindow.cpp` — `createPhotoPanel()`, slots

- [ ] **Step 1: Write `createPhotoPanel()` — before/after comparison**

```cpp
QWidget* MainWindow::createPhotoPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("图片翻译"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("将图片中的文字翻译为目标语言，生成翻译后的新图片"));
    hint->setObjectName("sectionHint");
    layout->addWidget(hint);

    // 输入路径
    auto* inputRow = new QHBoxLayout;
    photoInputPath_ = new QLineEdit;
    photoInputPath_->setPlaceholderText(QStringLiteral("选择图片…"));
    inputRow->addWidget(photoInputPath_);
    auto* inBrowseBtn = new QPushButton(QStringLiteral("浏览…"));
    inBrowseBtn->setObjectName("secondaryBtn");
    connect(inBrowseBtn, &QPushButton::clicked, this, &MainWindow::onSelectPhotoInput);
    inputRow->addWidget(inBrowseBtn);
    layout->addLayout(inputRow);

    // 左右对比预览
    auto* compareRow = new QHBoxLayout;
    compareRow->setSpacing(16);

    // 左 — 原图
    auto* inputCard = new QFrame;
    inputCard->setObjectName("card");
    auto* inputCardLayout = new QVBoxLayout(inputCard);
    inputCardLayout->addWidget(new QLabel(QStringLiteral("原图")));
    photoInputPreview_ = new QLabel;
    photoInputPreview_->setFixedSize(360, 240);
    photoInputPreview_->setAlignment(Qt::AlignCenter);
    photoInputPreview_->setStyleSheet("background:#F5F5F7; border-radius:4px; color:#86868B;");
    photoInputPreview_->setText(QStringLiteral("选择图片后预览"));
    auto* inputScroll = new QScrollArea;
    inputScroll->setWidget(photoInputPreview_);
    inputScroll->setFixedHeight(260);
    inputCardLayout->addWidget(inputScroll);
    compareRow->addWidget(inputCard);

    // 右 — 翻译后
    auto* outputCard = new QFrame;
    outputCard->setObjectName("card");
    auto* outputCardLayout = new QVBoxLayout(outputCard);
    outputCardLayout->addWidget(new QLabel(QStringLiteral("翻译后")));
    photoOutputPreview_ = new QLabel;
    photoOutputPreview_->setFixedSize(360, 240);
    photoOutputPreview_->setAlignment(Qt::AlignCenter);
    photoOutputPreview_->setStyleSheet("background:#F5F5F7; border-radius:4px; color:#86868B;");
    photoOutputPreview_->setText(QStringLiteral("翻译后图片将显示在这里"));
    auto* outputScroll = new QScrollArea;
    outputScroll->setWidget(photoOutputPreview_);
    outputScroll->setFixedHeight(260);
    outputCardLayout->addWidget(outputScroll);
    compareRow->addWidget(outputCard);
    layout->addLayout(compareRow);

    // 输出路径
    auto* outputRow = new QHBoxLayout;
    photoOutputPath_ = new QLineEdit;
    photoOutputPath_->setPlaceholderText(QStringLiteral("留空则自动生成（源文件名_trans）"));
    outputRow->addWidget(photoOutputPath_);
    auto* outBrowseBtn = new QPushButton(QStringLiteral("浏览…"));
    outBrowseBtn->setObjectName("secondaryBtn");
    connect(outBrowseBtn, &QPushButton::clicked, this, &MainWindow::onSelectPhotoOutput);
    outputRow->addWidget(outBrowseBtn);
    layout->addLayout(outputRow);

    // 参数
    auto* ctrlRow = new QHBoxLayout;
    ctrlRow->addWidget(new QLabel(QStringLiteral("目标语言:")));
    photoLangCombo_ = new QComboBox;
    photoLangCombo_->setEditable(true);
    photoLangCombo_->addItems({
        QStringLiteral("中文"), QStringLiteral("English"),
        QStringLiteral("日本語"), QStringLiteral("한국어")
    });
    photoLangCombo_->setCurrentText(QStringLiteral("中文"));
    ctrlRow->addWidget(photoLangCombo_);
    ctrlRow->addStretch();
    ctrlRow->addWidget(new QLabel(QStringLiteral("Max Tokens:")));
    photoMaxTokens_ = new QSpinBox;
    photoMaxTokens_->setRange(64, 4096);
    photoMaxTokens_->setValue(512);
    ctrlRow->addWidget(photoMaxTokens_);
    photoTranslateBtn_ = new QPushButton(QStringLiteral("翻译图片"));
    photoTranslateBtn_->setObjectName("primaryBtn");
    connect(photoTranslateBtn_, &QPushButton::clicked, this, &MainWindow::onProcessPhoto);
    ctrlRow->addWidget(photoTranslateBtn_);
    layout->addLayout(ctrlRow);

    return panel;
}
```

- [ ] **Step 2: Write slots**

```cpp
void MainWindow::onSelectPhotoInput() {
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择图片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.tiff);;所有文件 (*)"));
    if (path.isEmpty()) return;
    photoInputPath_->setText(path);
    QPixmap pix(path);
    if (!pix.isNull()) {
        photoInputPreview_->setPixmap(pix.scaled(360, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        photoInputPreview_->setText("");
    } else {
        photoInputPreview_->setText(QStringLiteral("无法加载预览"));
    }
    photoOutputPreview_->setText(QStringLiteral("翻译后图片将显示在这里"));
    photoOutputPreview_->setPixmap(QPixmap());
}

void MainWindow::onSelectPhotoOutput() {
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存翻译后图片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp);;所有文件 (*)"));
    if (!path.isEmpty()) photoOutputPath_->setText(path);
}

void MainWindow::onProcessPhoto() {
    if (busy_.loadRelaxed()) {
        statusBar_->showMessage(QStringLiteral("正在处理中，请等待…"), 3000);
        return;
    }
    QString inputPath = photoInputPath_->text().trimmed();
    if (inputPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择输入图片"));
        return;
    }

    auto* worker = new TranslateWorker;
    worker->mode = TranslateWorker::PhotoTranslate;
    worker->inputPath = inputPath;
    worker->outputPath = photoOutputPath_->text().trimmed();
    worker->targetLang = photoLangCombo_->currentText();
    worker->maxTokens = photoMaxTokens_->value();
    runWorker(worker);
}
```

- [ ] **Step 3: Update `onWorkerFinished` to show photo output**

In the switch, after case 0/2/4, add the photo output path display:
```cpp
case 3: // 图片翻译 — result is the output file path
{
    fileResultView_->setPlainText(result); // reuse for display
    QPixmap outPix(result);
    if (!outPix.isNull()) {
        photoOutputPreview_->setPixmap(outPix.scaled(360, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        photoOutputPreview_->setText("");
    }
    break;
}
```
Note: this needs to be in the onWorkerFinished switch, which already routes by currentNavIndex_.

---

### Task 7: Settings Panel

**Files:**
- Modify: `ui/mainwindow.cpp` — `createSettingsPanel()`

- [ ] **Step 1: Write `createSettingsPanel()`**

```cpp
QWidget* MainWindow::createSettingsPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("设置"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    // 引擎配置卡片
    auto* configCard = new QFrame;
    configCard->setObjectName("card");
    auto* configForm = new QFormLayout(configCard);

    auto* baseDirRow = new QHBoxLayout;
    baseDirPath_ = new QLineEdit;
    baseDirPath_->setPlaceholderText(QStringLiteral("留空则使用可执行文件所在目录"));
    baseDirRow->addWidget(baseDirPath_);
    auto* browseBtn = new QPushButton(QStringLiteral("浏览…"));
    browseBtn->setObjectName("secondaryBtn");
    connect(browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseBaseDir);
    baseDirRow->addWidget(browseBtn);
    configForm->addRow(QStringLiteral("模型/DLL 目录:"), baseDirRow);

    configStatusLabel_ = new QLabel(QStringLiteral("启动时自动加载 config.json"));
    configStatusLabel_->setStyleSheet("color:#86868B; font-size:12px;");
    configForm->addRow(QStringLiteral("状态:"), configStatusLabel_);
    layout->addWidget(configCard);

    // 关于卡片
    auto* aboutCard = new QFrame;
    aboutCard->setObjectName("card");
    auto* aboutLayout = new QVBoxLayout(aboutCard);
    auto* aboutTitle = new QLabel(QStringLiteral("关于 AceTranslatePro"));
    aboutTitle->setObjectName("sectionTitle");
    aboutLayout->addWidget(aboutTitle);
    auto* aboutText = new QLabel(
        QStringLiteral("AceTranslatePro v1.0\n"
                       "基于版面分析 + OCR + VLM + 翻译引擎的文档翻译工具\n\n"
                       "支持的格式: 图片, PDF, Office (docx/xlsx/pptx), Markdown, 纯文本"));
    aboutText->setWordWrap(true);
    aboutText->setStyleSheet("font-size:13px; color:#86868B; line-height:1.6;");
    aboutLayout->addWidget(aboutText);
    layout->addWidget(aboutCard);

    layout->addStretch();
    return panel;
}

void MainWindow::onBrowseBaseDir() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择模型/DLL 目录"));
    if (!dir.isEmpty()) baseDirPath_->setText(dir);
}
```

---

### Task 8: Float Translate Config Panel (划词翻译页面)

**Files:**
- Modify: `ui/mainwindow.cpp` — `createFloatConfigPanel()`, slots

- [ ] **Step 1: Write `createFloatConfigPanel()`**

```cpp
QWidget* MainWindow::createFloatConfigPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("划词翻译"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("选中任意应用中的文字，按快捷键弹出翻译窗口"));
    hint->setObjectName("sectionHint");
    layout->addWidget(hint);

    // 配置卡片
    auto* configCard = new QFrame;
    configCard->setObjectName("card");
    auto* configForm = new QFormLayout(configCard);
    configForm->setSpacing(12);

    // 快捷键
    auto* hotkeyRow = new QHBoxLayout;
    floatHotkeyEdit_ = new QLineEdit(QStringLiteral("Ctrl+Shift+C"));
    floatHotkeyEdit_->setReadOnly(true);
    floatHotkeyEdit_->setMaximumWidth(200);
    hotkeyRow->addWidget(floatHotkeyEdit_);
    hotkeyRow->addStretch();
    configForm->addRow(QStringLiteral("全局快捷键:"), hotkeyRow);

    floatLangCombo_ = new QComboBox;
    floatLangCombo_->setEditable(true);
    floatLangCombo_->addItems({
        QStringLiteral("中文"), QStringLiteral("English"),
        QStringLiteral("日本語"), QStringLiteral("한국어")
    });
    floatLangCombo_->setCurrentText(QStringLiteral("中文"));
    configForm->addRow(QStringLiteral("默认语言:"), floatLangCombo_);

    floatAutoCopy_ = new QCheckBox(QStringLiteral("翻译后自动复制译文到剪贴板"));
    floatAutoCopy_->setChecked(true);
    configForm->addRow(QStringLiteral(""), floatAutoCopy_);

    floatStayOnTop_ = new QCheckBox(QStringLiteral("窗口置顶"));
    floatStayOnTop_->setChecked(true);
    connect(floatStayOnTop_, &QCheckBox::toggled, this, &MainWindow::onFloatWindowConfigChanged);
    configForm->addRow(QStringLiteral(""), floatStayOnTop_);

    layout->addWidget(configCard);

    // 翻译历史卡片
    auto* historyCard = new QFrame;
    historyCard->setObjectName("card");
    auto* historyLayout = new QVBoxLayout(historyCard);
    auto* historyTitleRow = new QHBoxLayout;
    historyTitleRow->addWidget(new QLabel(QStringLiteral("翻译历史")));
    historyTitleRow->addStretch();
    auto* clearBtn = new QPushButton(QStringLiteral("清空"));
    clearBtn->setObjectName("secondaryBtn");
    historyTitleRow->addWidget(clearBtn);
    historyLayout->addLayout(historyTitleRow);

    floatHistoryView_ = new QTextEdit;
    floatHistoryView_->setReadOnly(true);
    floatHistoryView_->setPlaceholderText(QStringLiteral("划词翻译记录将显示在这里…"));
    floatHistoryView_->setMaximumHeight(200);
    historyLayout->addWidget(floatHistoryView_);
    layout->addWidget(historyCard);

    layout->addStretch();
    return panel;
}

void MainWindow::onFloatWindowConfigChanged() {
    if (floatWindow_) {
        floatWindow_->setStayOnTop(floatStayOnTop_->isChecked());
    }
}
```

---

### Task 9: RegionCapture — Screenshot Region Selector

**Files:**
- Create: `ui/regioncapture.h`
- Create: `ui/regioncapture.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: `RegionCapture` class with:
  - `void startCapture()` — begin fullscreen capture mode
  - `cv::Mat capturedImage()` — returns the captured region as cv::Mat
  - `signal regionCaptured()` — emitted when region is selected

- [ ] **Step 1: Write `ui/regioncapture.h`**

```cpp
#pragma once

#include <QWidget>
#include <QPoint>
#include <opencv2/opencv.hpp>

/**
 * @brief 全屏截图选区窗口
 *
 * 创建全屏半透明遮罩，用户拖拽选择区域，按 Esc 取消。
 * 选择完成后发射 regionCaptured() 信号，调用 capturedImage() 获取截图。
 */
class RegionCapture : public QWidget {
    Q_OBJECT
public:
    explicit RegionCapture(QWidget* parent = nullptr);
    ~RegionCapture() override = default;

    void startCapture();
    cv::Mat capturedImage() const { return capturedMat_; }

signals:
    void regionCaptured();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPixmap fullScreenPixmap_;
    QPoint startPoint_;
    QPoint endPoint_;
    bool selecting_ = false;
    bool captured_ = false;
    cv::Mat capturedMat_;
};
```

- [ ] **Step 2: Write `ui/regioncapture.cpp`**

```cpp
#include "regioncapture.h"
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGuiApplication>

RegionCapture::RegionCapture(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setCursor(Qt::CrossCursor);
}

void RegionCapture::startCapture() {
    // 捕获全屏
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    fullScreenPixmap_ = screen->grabWindow(0);
    captured_ = false;
    capturedMat_ = cv::Mat();

    // 设置全屏遮罩
    QRect screenGeom = screen->geometry();
    setGeometry(screenGeom);
    showFullScreen();
}

void RegionCapture::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    // 绘制半透明全屏截图
    painter.drawPixmap(0, 0, fullScreenPixmap_);

    // 绘制半透明遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 120));

    if (selecting_) {
        QRect selRect = QRect(startPoint_, endPoint_).normalized();
        // 绘制选中区域（恢复原图）
        painter.drawPixmap(selRect, fullScreenPixmap_, selRect);
        // 绘制选区边框
        QPen pen(QColor(0, 122, 255), 2, Qt::DashLine);
        painter.setPen(pen);
        painter.drawRect(selRect);

        // 显示尺寸信息
        QString sizeInfo = QStringLiteral("%1 × %2").arg(selRect.width()).arg(selRect.height());
        painter.setPen(Qt::white);
        painter.setFont(QFont("Microsoft YaHei", 10));
        painter.drawText(selRect.topLeft() + QPoint(4, -4), sizeInfo);
    }
}

void RegionCapture::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        startPoint_ = event->pos();
        endPoint_ = startPoint_;
        selecting_ = true;
        update();
    }
}

void RegionCapture::mouseMoveEvent(QMouseEvent* event) {
    if (selecting_) {
        endPoint_ = event->pos();
        update();
    }
}

void RegionCapture::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && selecting_) {
        selecting_ = false;
        QRect selRect = QRect(startPoint_, endPoint_).normalized();
        if (selRect.width() < 10 || selRect.height() < 10) {
            // 选区太小，取消
            hide();
            return;
        }

        // 从全屏截图中截取选中区域
        QPixmap region = fullScreenPixmap_.copy(selRect);
        QImage qimg = region.toImage().convertToFormat(QImage::Format_BGR888);

        // 转换为 cv::Mat
        capturedMat_ = cv::Mat(qimg.height(), qimg.width(), CV_8UC3,
                               const_cast<uchar*>(qimg.bits()),
                               static_cast<size_t>(qimg.bytesPerLine())).clone();

        captured_ = true;
        hide();
        emit regionCaptured();
    }
}

void RegionCapture::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        selecting_ = false;
        captured_ = false;
        capturedMat_ = cv::Mat();
        hide();
    }
}
```

---

### Task 10: FloatTranslateWindow — Floating Translate Tool

**Files:**
- Create: `ui/floatwindow.h`
- Create: `ui/floatwindow.cpp`

**Interfaces:**
- Consumes: `translate_text()` from `docmind/DocumentEngine.h`
- Consumes: `RegisterHotKey`/`UnregisterHotKey` Win32 API
- Produces: `FloatTranslateWindow` class

- [ ] **Step 1: Write `ui/floatwindow.h`**

```cpp
#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QPoint>
#include <windows.h>

class FloatTranslateWindow : public QWidget {
    Q_OBJECT
public:
    explicit FloatTranslateWindow(QWidget* parent = nullptr);
    ~FloatTranslateWindow() override;

    void setStayOnTop(bool on);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onTranslate();
    void onCopyResult();
    void onToggleLock();

private:
    void registerHotKey();
    void unregisterHotKey();
    void performTranslate(const QString& text);

    QLabel* sourceText_;
    QTextEdit* resultText_;
    QComboBox* langCombo_;
    QPushButton* translateBtn_;
    QPushButton* copyBtn_;
    QPushButton* lockBtn_;
    QPushButton* closeBtn_;

    bool locked_ = false;
    bool dragging_ = false;
    QPoint dragStartPos_;

    // Win32 热键 ID
    static constexpr int HOTKEY_ID = 0xACE1;
};
```

- [ ] **Step 2: Write `ui/floatwindow.cpp`**

```cpp
#include "floatwindow.h"
#include "docmind/DocumentEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QStyleOption>

FloatTranslateWindow::FloatTranslateWindow(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint) {

    setFixedSize(380, 240);
    setObjectName("floatWindow");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- 标题栏（可拖动） ----
    auto* titleBar = new QWidget;
    titleBar->setFixedHeight(36);
    titleBar->setStyleSheet("background:#007AFF; border-top-left-radius:8px; border-top-right-radius:8px;");
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 8, 0);

    auto* iconLabel = new QLabel(QStringLiteral("✂️ 翻译"));
    iconLabel->setStyleSheet("color:white; font-size:13px; font-weight:bold;");
    titleLayout->addWidget(iconLabel);

    titleLayout->addStretch();

    lockBtn_ = new QPushButton(QStringLiteral("📌"));
    lockBtn_->setFixedSize(28, 28);
    lockBtn_->setStyleSheet("background:transparent; color:white; border:none; font-size:14px;");
    lockBtn_->setToolTip(QStringLiteral("置顶 / 取消置顶"));
    connect(lockBtn_, &QPushButton::clicked, this, &FloatTranslateWindow::onToggleLock);
    titleLayout->addWidget(lockBtn_);

    closeBtn_ = new QPushButton(QStringLiteral("✕"));
    closeBtn_->setFixedSize(28, 28);
    closeBtn_->setStyleSheet("background:transparent; color:white; border:none; font-size:14px;");
    closeBtn_->setToolTip(QStringLiteral("关闭"));
    connect(closeBtn_, &QPushButton::clicked, this, &QWidget::hide);
    titleLayout->addWidget(closeBtn_);

    mainLayout->addWidget(titleBar);

    // ---- 内容区 ----
    auto* content = new QWidget;
    content->setStyleSheet("background:white; border-bottom-left-radius:8px; border-bottom-right-radius:8px;");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(12, 10, 12, 10);
    contentLayout->setSpacing(8);

    // 原文
    sourceText_ = new QLabel(QStringLiteral("选中文本后将显示在这里"));
    sourceText_->setWordWrap(true);
    sourceText_->setStyleSheet("font-size:13px; color:#1D1D1F; padding:4px 0;");
    sourceText_->setMaximumHeight(40);
    contentLayout->addWidget(sourceText_);

    // 分割线
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#E5E5EA;");
    contentLayout->addWidget(sep);

    // 译文
    resultText_ = new QTextEdit;
    resultText_->setReadOnly(true);
    resultText_->setPlaceholderText(QStringLiteral("译文"));
    resultText_->setStyleSheet("border:none; font-size:13px; color:#1D1D1F; background:transparent;");
    contentLayout->addWidget(resultText_);

    // 底部操作栏
    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(6);

    langCombo_ = new QComboBox;
    langCombo_->addItems({QStringLiteral("中文"), QStringLiteral("English"),
                          QStringLiteral("日本語"), QStringLiteral("한국어")});
    langCombo_->setCurrentText(QStringLiteral("中文"));
    langCombo_->setStyleSheet("border:1px solid #D1D1D6; border-radius:4px; padding:4px 8px; font-size:12px;");
    bottomRow->addWidget(langCombo_);

    translateBtn_ = new QPushButton(QStringLiteral("翻译"));
    translateBtn_->setStyleSheet(
        "background:#007AFF; color:white; border:none; border-radius:4px; padding:4px 16px; font-size:12px;");
    connect(translateBtn_, &QPushButton::clicked, this, &FloatTranslateWindow::onTranslate);
    bottomRow->addWidget(translateBtn_);

    copyBtn_ = new QPushButton(QStringLiteral("复制"));
    copyBtn_->setStyleSheet(
        "background:transparent; color:#007AFF; border:1px solid #007AFF; border-radius:4px; padding:4px 12px; font-size:12px;");
    connect(copyBtn_, &QPushButton::clicked, this, &FloatTranslateWindow::onCopyResult);
    bottomRow->addWidget(copyBtn_);

    bottomRow->addStretch();
    contentLayout->addLayout(bottomRow);

    mainLayout->addWidget(content, 1);

    // 注册全局热键
    registerHotKey();
}

FloatTranslateWindow::~FloatTranslateWindow() {
    unregisterHotKey();
}

void FloatTranslateWindow::setStayOnTop(bool on) {
    if (on) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    }
    show();
}

void FloatTranslateWindow::registerHotKey() {
    // 注册 Ctrl+Shift+C (MOD_CONTROL | MOD_SHIFT, 'C')
    if (!RegisterHotKey(reinterpret_cast<HWND>(winId()), HOTKEY_ID,
                        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'C')) {
        qWarning("Failed to register hotkey");
    }
}

void FloatTranslateWindow::unregisterHotKey() {
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), HOTKEY_ID);
}

bool FloatTranslateWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == HOTKEY_ID) {
        // 全局热键触发 — 从剪贴板读取选中文本
        HWND hwnd = GetClipboardOwner();
        if (OpenClipboard(nullptr)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
                if (pText) {
                    QString text = QString::fromWCharArray(pText);
                    GlobalUnlock(hData);
                    CloseClipboard();
                    // 在翻译之前清除剪贴板（避免重复触发）
                    if (!text.isEmpty()) {
                        performTranslate(text);
                    }
                } else {
                    CloseClipboard();
                }
            } else {
                CloseClipboard();
            }
        }
        return true;
    }
    return QWidget::nativeEvent(eventType, message, result);
}

void FloatTranslateWindow::performTranslate(const QString& text) {
    sourceText_->setText(text.left(200)); // 截断显示
    resultText_->setPlainText(QStringLiteral("翻译中…"));

    try {
        std::string result = translate_text(
            text.toStdString(),
            langCombo_->currentText().toStdString(),
            512);
        QString translated = QString::fromStdString(result);
        resultText_->setPlainText(translated);
        show();
        raise();
        activateWindow();
    } catch (const std::exception& e) {
        resultText_->setPlainText(QStringLiteral("翻译出错: ") + QString::fromUtf8(e.what()));
    }
}

void FloatTranslateWindow::onTranslate() {
    QString text = sourceText_->text();
    if (!text.isEmpty() && text != QStringLiteral("选中文本后将显示在这里")) {
        performTranslate(text);
    }
}

void FloatTranslateWindow::onCopyResult() {
    QApplication::clipboard()->setText(resultText_->toPlainText());
}

void FloatTranslateWindow::onToggleLock() {
    locked_ = !locked_;
    lockBtn_->setText(locked_ ? QStringLiteral("🔓") : QStringLiteral("📌"));
    lockBtn_->setToolTip(locked_ ? QStringLiteral("点击解锁可移动") : QStringLiteral("点击锁定"));
}

void FloatTranslateWindow::paintEvent(QPaintEvent* event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

// 窗口拖动
void FloatTranslateWindow::mousePressEvent(QMouseEvent* event) {
    if (event->y() <= 36 && !locked_) {
        dragging_ = true;
        dragStartPos_ = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void FloatTranslateWindow::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        move(event->globalPos() - dragStartPos_);
        event->accept();
    }
}

void FloatTranslateWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
        event->accept();
    }
}

void FloatTranslateWindow::closeEvent(QCloseEvent* event) {
    hide(); // 只是隐藏，不销毁
    event->ignore();
}
```

---

### Task 11: MainWindow Integration — Connect All Pieces

**Files:**
- Modify: `ui/mainwindow.cpp` — ensure `setupUI()` constructor and all wiring works

- [ ] **Step 1: Verify MainWindow constructor wires RegionCapture and FloatTranslateWindow**

```cpp
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // 截图区域捕获
    regionCapture_ = new RegionCapture(this);
    connect(regionCapture_, &RegionCapture::regionCaptured,
            this, &MainWindow::onTranslateScreenshotLocal);

    // 划词翻译悬浮窗（无父窗口 — 独立顶层窗口）
    floatWindow_ = new FloatTranslateWindow(nullptr);

    // 监听设置变更
    connect(floatStayOnTop_, &QCheckBox::toggled,
            this, &MainWindow::onFloatWindowConfigChanged);

    setupUI();
    resize(1000, 720);
    onNavButtonClicked(0); // 默认选中文本翻译
}

// 析构时确保清理悬浮窗
MainWindow::~MainWindow() {
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(3000);
    }
    // floatWindow_ 无父窗口，手动删除
    delete floatWindow_;
}
```

- [ ] **Step 2: Verify `onWorkerFinished` integration with photo result display**

The switch in onWorkerFinished needs to handle all 5 panel indices. The final version:

```cpp
void MainWindow::onWorkerFinished(const QString& result) {
    progressBar_->hide();
    statusBar_->showMessage(QStringLiteral("完成"), 5000);

    switch (currentNavIndex_) {
    case 0: // 文本翻译
        textOutput_->setPlainText(result);
        break;
    case 2: // 截图翻译
        screenshotResult_->setPlainText(result);
        break;
    case 3: // 图片翻译 — result is output path
        fileResultView_->setPlainText(result);
        {
            QPixmap outPix(result);
            if (!outPix.isNull()) {
                photoOutputPreview_->setPixmap(
                    outPix.scaled(360, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                photoOutputPreview_->setText("");
            }
        }
        break;
    case 4: // 文件翻译
        fileResultView_->setPlainText(result);
        break;
    }

    busy_.storeRelaxed(0);
    enableAllButtons(true);
}

void MainWindow::enableAllButtons(bool enabled) {
    if (textTranslateBtn_) textTranslateBtn_->setEnabled(enabled);
    if (fileProcessBtn_) fileProcessBtn_->setEnabled(enabled);
    if (screenshotCaptureBtn_) screenshotCaptureBtn_->setEnabled(enabled);
    if (photoTranslateBtn_) photoTranslateBtn_->setEnabled(enabled);
}
```

- [ ] **Step 3: Final build check**

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build .
```

Expected: clean compile, zero warnings.

---

## Self-Review Checklist

- [ ] **Spec coverage:** All 5 modules covered (text, float, screenshot, photo, file) + settings. QSS spec colors/margins matched. RegionCapture and FloatTranslateWindow implemented per spec.
- [ ] **No placeholders:** All code blocks contain complete, compilable implementations. No "TBD" or "TODO" in the plan.
- [ ] **Type consistency:** `RegionCapture::capturedImage()` returns `cv::Mat`, consumed by `TranslateWorker::screenshotMat` — consistent. `translate_text()` / `process_file()` / `process_photo()` / `translate_screenshot_image()` signatures match existing `DocumentEngine.h`.
- [ ] **File paths:** All paths use `ui/` relative to project root. All new files added to CMakeLists.txt.

## Verification

1. **Build:** `cd build && cmake .. && cmake --build .` — must compile with zero errors
2. **Nav switching:** Launch → click each of 6 nav buttons → verify stacked widget switches
3. **Text translate:** Enter text, select language, click translate → result appears
4. **Screenshot capture:** Click [📸 截图翻译] → fullscreen overlay → drag region → result shows
5. **Photo translation:** Select input → translate → right preview shows output image (or file path)
6. **File translation:** Select file → translate → result path shown
7. **Float translate:** Press Ctrl+Shift+C anywhere → floating window appears with translation
8. **QSS check:** All buttons/inputs have flat style, hover/pressed states, correct colors
