#pragma once

#include <QMainWindow>
#include <QAbstractNativeEventFilter>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QStackedWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QFrame>
#include <QVector>
#include <QProgressBar>
#include <QStatusBar>
#include <QThread>
#include <QAtomicInt>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPropertyAnimation>
#include "zoomablelabel.h"

#include "zoomablelabel.h"
#include "docmind/core/ConfigManager.hpp"
#include <QTimer>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// Undefine winsock's connect macro so Qt's connect works
#ifdef connect
#undef connect
#endif
#include <opencv2/opencv.hpp>

// 用于读写配置文件
#include "docmind/core/ConfigManager.hpp"

class RegionCapture;
class FloatTranslateWindow;

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
    QString currentFilePath; // 用于文件翻译时显示进度
    float layoutThreshold = 0.5f;
    int pdfDpi = 200;
    int maxTokens = 512;
    bool enableWarp = true;
    bool enableEnhance = false;
    cv::Mat screenshotMat;
    QString ocrOutput; // 截图 OCR 识别结果

public slots:
    void run();

signals:
    void progressUpdated(const QString& msg);
    void finished(const QString& result);
    void ocrFinished(const QString& ocrText);
    void errorOccurred(const QString& err);
};

// ============================================================
// DropZoneWidget — 拖拽文件放置区域
// ============================================================
class DropZoneWidget : public QFrame {
    Q_OBJECT
public:
    explicit DropZoneWidget(QWidget* parent = nullptr);
signals:
    void fileDropped(const QStringList& paths);
protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
};

// ============================================================
// 主窗口
// ============================================================
class MainWindow : public QMainWindow, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // 导航
    void onNavButtonClicked(int index);

    // 文本翻译
    void onTranslateText();
    void onMicButtonClicked();
    void resetMicButton();
    // 文件翻译
    void onSelectInputFile();
    void onProcessFile();
    // 截图翻译
    void onCaptureComplete();
    // 图片翻译 (Photo)
    void onSelectPhotoInput();
    // photoOutputPath_ 已移除
    void onProcessPhoto();
    // onSelectOutputDir 已移除
    // 设置
    void onBrowseBaseDir();
    // 工作线程回调
    void onWorkerFinished(const QString& result);
    void onWorkerError(const QString& err);
    void onWorkerProgress(const QString& msg);
    void onHourglassTick();

    // 全局热键
    void onFloatHotkey();
    void onScreenshotHotkey();

    // 语言切换
    static void switchLanguage(const QString& langCode);

    // 系统托盘
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowWindow();
    void onQuitApp();

private:
    void setupUI();
    void addFileToList(const QString& filePath);  // 文件面板辅助
    void createTrayIcon();
    QWidget* createNavButton(int index, const QString& iconPath, const QString& label);
    QWidget* createTextPanel();
    QWidget* createFloatConfigPanel();
    QWidget* createScreenshotPanel();
    QWidget* createPhotoPanel();
    QWidget* createFilePanel();
    QWidget* createSettingsPanel();
    void runWorker(TranslateWorker* worker);
    void registerGlobalHotkeys();
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    static QPixmap createCheckIcon();
    void speakText(const QString& text);

    // -- 导航 --
    QFrame* navPanel_;
    QStackedWidget* stackedWidget_;
    QVector<QPushButton*> navButtons_;
    int currentNavIndex_ = 0;

    // -- 通用 --
    QStatusBar* statusBar_;
    QLabel* statusIcon_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progressBar_;
    QThread* workerThread_ = nullptr;
    TranslateWorker* currentWorker_ = nullptr;
    QAtomicInt busy_ = 0;
    QTimer* hourglassTimer_ = nullptr;
    int hourglassFrame_ = 0;

    // -- 文本翻译 --
    QPlainTextEdit* textInput_;
    QPlainTextEdit* textOutput_;
    QComboBox* textLangCombo_;
    QSpinBox* textMaxTokens_;
    QPushButton* textTranslateBtn_;
    // 文本面板新增组件
    QPushButton* textSourceLangBtn_;
    QPushButton* textTargetLangBtn_;
    QPushButton* textSwapLangBtn_;
    QLabel* textCharCountInput_;
    QLabel* textCharCountOutput_;
    QPushButton* textPasteBtn_;
    QPushButton* textClearBtn_;
    QPushButton* textCopyBtn_;
    QPushButton* textSpeakBtn_;
    QPushButton* textMicBtn_ = nullptr;

    // -- 文件翻译 --
    QLineEdit* fileInputPath_;
    // fileOutputDir_ 已移除
    QComboBox* fileLangCombo_;
    QDoubleSpinBox* fileLayoutThreshold_;
    QSpinBox* filePdfDpi_;
    QCheckBox* fileEnableWarp_;
    QCheckBox* fileEnableEnhance_;
    QTextEdit* fileResultView_;
    QPushButton* fileProcessBtn_;
    // 文件面板新增组件
    DropZoneWidget* fileDropZone_;
    QWidget* fileListContainer_;
    QVBoxLayout* fileListLayout_;
    QPushButton* fileTranslateBtn_;
    QStringList filePendingPaths_;
    int fileCurrentIdx_ = 0;

    // -- 截图翻译 --
    QPushButton* screenshotCaptureBtn_;
    ZoomableLabel* screenshotPreview_;
    QComboBox* screenshotLangCombo_;
    QSpinBox* screenshotMaxTokens_;
    QTextEdit* screenshotResult_;
    QTextEdit* screenshotOcrResult_ = nullptr;
    QPixmap screenshotFullPix_;
    QScrollArea* screenshotScroll_ = nullptr;

    // -- 图片翻译 (Photo) --
    QLineEdit* photoInputPath_;
    QComboBox* photoLangCombo_;
    QSpinBox* photoMaxTokens_;
    QPushButton* photoCopyBtn_;
    QPushButton* photoDownloadBtn_;
    QString photoLastSavedPath_;
    ZoomableLabel* photoPreview_;
    ZoomableLabel* photoOutputPreview_;
    QScrollArea* photoInputScroll_;
    QScrollArea* photoOutputScroll_;
    QPushButton* photoTranslateBtn_;
    cv::Mat photoOrigMat_;      // 原图 cv::Mat 用于缩放
    cv::Mat photoOutputMat_;    // 翻译后 cv::Mat 用于缩放
    double photoZoom_ = 1.0;    // 当前缩放比例

    // -- 设置 --
    QLineEdit* baseDirPath_;
    QLabel* configStatusLabel_;

    // -- 区域截图 & 浮动窗口 --
    RegionCapture* regionCapture_;
    FloatTranslateWindow* floatWindow_;
    QTextEdit* floatHistoryView_ = nullptr;

    // -- 全局热键 --
    qintptr floatHotkeyId_ = 0xACE1;
    qintptr screenshotHotkeyId_ = 0xACE2;
    int floatHotkeyMods_ = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
    int floatHotkeyKey_ = 'C';
    int screenshotHotkeyMods_ = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
    int screenshotHotkeyKey_ = 'Z';
    QPushButton* floatHotkeyBtn_ = nullptr;
    QPushButton* screenshotHotkeyBtn_ = nullptr;

    // -- 系统托盘 --
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;

    // -- 各模块卡片 (用于悬浮高亮) --
    QFrame* textSourceCard_ = nullptr;
    QFrame* textResultCard_ = nullptr;
    QFrame* screenshotPreviewCard_ = nullptr;
    QFrame* screenshotResultCard_ = nullptr;
    QFrame* photoInputCard_ = nullptr;
    QFrame* photoOutputCard_ = nullptr;
    QFrame* floatConfigCard_ = nullptr;
    QFrame* historyCard_ = nullptr;

    // -- 语音输入 --
    bool isRecording_ = false;
    QThread* asrThread_ = nullptr;
};
