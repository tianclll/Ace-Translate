#include "mainwindow.h"
#include "docmind/DocumentEngine.h"
#include "docmind/modules/PhotoTranslationModule.hpp"
#include "docmind/core/GlobalEngineContext.hpp"
#include "regioncapture.h"
#include "floatwindow.h"
#include "toast.h"
#include "zoomablelabel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QPixmap>
#include <QApplication>
#include <QDebug>
#include <QFrame>
#include <QComboBox>
#include <QStackedWidget>
#include <QSplitter>
#include <QDialog>
#include <QKeySequenceEdit>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QDateTime>
#include <QDateTime>
#include <QCloseEvent>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QCompleter>
#include <QStringListModel>

#include <QTextCursor>
#include <QThread>
#include <QIcon>
#include <QTranslator>

#include <sapi.h>
#include <sphelper.h>
#include <sstream>
#include <mmsystem.h>

// 前向声明：waveIn 回调
static void CALLBACK WaveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                 DWORD_PTR dwParam1, DWORD_PTR dwParam2);

#include <opencv2/opencv.hpp>

// ============================================================
// 静态辅助：为下拉框填充语言
// ============================================================
static void populateLanguages(QComboBox* combo, const QString& defaultLang = QStringLiteral("Chinese"), bool useConfigDefault = false) {
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    QStringList langs = {
        QApplication::translate("MainWindow", "Chinese"),
        QApplication::translate("MainWindow", "English"),
        QApplication::translate("MainWindow", "French"),
        QApplication::translate("MainWindow", "Portuguese"),
        QApplication::translate("MainWindow", "Spanish"),
        QApplication::translate("MainWindow", "Japanese"),
        QApplication::translate("MainWindow", "Turkish"),
        QApplication::translate("MainWindow", "Russian"),
        QApplication::translate("MainWindow", "Arabic"),
        QApplication::translate("MainWindow", "Korean"),
        QApplication::translate("MainWindow", "Thai"),
        QApplication::translate("MainWindow", "Italian"),
        QApplication::translate("MainWindow", "German"),
        QApplication::translate("MainWindow", "Vietnamese"),
        QApplication::translate("MainWindow", "Malay"),
        QApplication::translate("MainWindow", "Indonesian"),
        QApplication::translate("MainWindow", "Filipino"),
        QApplication::translate("MainWindow", "Hindi"),
        QApplication::translate("MainWindow", "Traditional Chinese"),
        QApplication::translate("MainWindow", "Polish"),
        QApplication::translate("MainWindow", "Czech"),
        QApplication::translate("MainWindow", "Dutch"),
        QApplication::translate("MainWindow", "Khmer"),
        QApplication::translate("MainWindow", "Burmese"),
        QApplication::translate("MainWindow", "Persian"),
        QApplication::translate("MainWindow", "Gujarati"),
        QApplication::translate("MainWindow", "Urdu"),
        QApplication::translate("MainWindow", "Telugu"),
        QApplication::translate("MainWindow", "Marathi"),
        QApplication::translate("MainWindow", "Hebrew"),
        QApplication::translate("MainWindow", "Bengali"),
        QApplication::translate("MainWindow", "Tamil"),
        QApplication::translate("MainWindow", "Ukrainian"),
        QApplication::translate("MainWindow", "Tibetan"),
        QApplication::translate("MainWindow", "Kazakh"),
        QApplication::translate("MainWindow", "Mongolian"),
        QApplication::translate("MainWindow", "Uyghur"),
        QApplication::translate("MainWindow", "Cantonese"),
    };
    combo->addItems(langs);
    if (useConfigDefault) {
        auto& cfg = docmind::ConfigManager::getInstance();
        combo->setCurrentText(QString::fromStdString(cfg.getDefaultLanguage()));
    } else {
        combo->setCurrentText(defaultLang);
    }

    // 模糊搜索自动补全：输入时过滤匹配项
    auto* completer = new QCompleter(langs, combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setMaxVisibleItems(10);
    combo->setCompleter(completer);

    // 失去焦点或确认时，只允许下拉列表中的值
    auto validateInput = [combo, langs]() {
        QString text = combo->currentText().trimmed();
        if (text.isEmpty()) return;
        for (const auto& lang : langs) {
            if (lang.compare(text, Qt::CaseInsensitive) == 0) {
                // 统一大小写
                combo->setCurrentText(lang);
                return;
            }
        }
        // 输入不在列表中，回退到列表第一个
        combo->setCurrentIndex(0);
    };
    // combo 的 lineEdit 在 setEditable(true) 后可用
    if (auto* lineEdit = combo->lineEdit()) {
        QObject::connect(lineEdit, &QLineEdit::editingFinished, combo, validateInput);
    }
}

// 递归复制目录（下载时同时复制 assets）
static bool copyDirectory(const QString& src, const QString& dst) {
    QDir srcDir(src);
    if (!srcDir.exists()) return false;
    QDir dstDir(dst);
    if (!dstDir.exists()) dstDir.mkpath(".");
    bool ok = true;
    for (const QFileInfo& info : srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (info.isDir()) {
            ok &= copyDirectory(info.absoluteFilePath(), dst + "/" + info.fileName());
        } else {
            ok &= QFile::copy(info.absoluteFilePath(), dst + "/" + info.fileName());
        }
    }
    return ok;
}

// ============================================================
// TranslateWorker — 在工作线程中调用翻译接口
// ============================================================
void TranslateWorker::run() {
    try {
            switch (mode) {
            case TextTranslate: {
                emit progressUpdated(tr("Translating…"));
                try {
                    std::string result = translate_text(
                        inputText.toStdString(),
                        targetLang.toStdString(),
                        maxTokens);
                    emit finished(QString::fromStdString(result));
                } catch (const std::exception& e) {
                    emit errorOccurred(tr("Translation engine error: ") + QString::fromUtf8(e.what()));
                }
                break;
            }
            case FileTranslate: {
                emit progressUpdated(tr("Processing: ") + currentFilePath);
                try {
                    std::string out = process_file(
                        inputPath.toStdString(),
                        outputPath.toStdString(),
                        baseDir.toStdString(),
                        targetLang.toStdString(),
                        layoutThreshold,
                        pdfDpi,
                        enableWarp,
                        enableEnhance);
                    emit finished(QString::fromStdString(out));
                } catch (const std::exception& e) {
                    emit errorOccurred(tr("File translation error: ") + QString::fromUtf8(e.what()));
                }
                break;
            }
            case ScreenshotTranslate: {
                emit progressUpdated(tr("Recognizing…"));
                try {
                    cv::Mat img = screenshotMat.clone();
                    if (img.empty()) {
                        emit errorOccurred(tr("Screenshot is empty"));
                        return;
                    }
                    // 先做 OCR 获取原文
                    docmind::GlobalEngineContext::getInstance().ensureOCREngine();
                    docmind::GlobalEngineContext::getInstance().ensureTranslatorEngine();
                    QString ocrText;
                    {
                        auto* ocr = docmind::GlobalEngineContext::getInstance().getOCREngine();
                        if (ocr) {
                            auto results = ocr->recognizeBuffer(img);
                            QStringList texts;
                            for (const auto& r : results) {
                                if (!r.text.empty())
                                    texts << QString::fromUtf8(r.text.c_str());
                            }
                            ocrText = texts.join("\n");
                        }
                        ocrOutput = ocrText;
                        emit ocrFinished(ocrText);
                    }
                    emit progressUpdated(tr("Translating…"));
                    // 再用翻译引擎
                    auto* translator = docmind::GlobalEngineContext::getInstance().getTranslatorEngine();
                    std::string result;
                    if (translator && !ocrText.isEmpty()) {
                        result = translator->translate(ocrText.toStdString(), targetLang.toStdString(), maxTokens);
                    }
                    emit finished(QString::fromStdString(result));
                } catch (const std::exception& e) {
                    emit errorOccurred(tr("Screenshot translation error: ") + QString::fromUtf8(e.what()));
                }
                break;
            }
            case PhotoTranslate: {
                emit progressUpdated(tr("Translating image…"));
                try {
                    cv::Mat inputImg = cv::imread(inputPath.toStdString());
                    if (inputImg.empty()) {
                        emit errorOccurred(tr("Cannot load image"));
                        return;
                    }
                    docmind::PhotoTranslationModule module(targetLang.toStdString(), "", 1.2f);
                    cv::Mat result = module.process(inputImg, maxTokens);
                    if (result.empty()) {
                        emit errorOccurred(tr("Image translation failed"));
                        return;
                    }
                    // 通过 finished 传递路径，onWorkerFinished 读取 cv::Mat
                    // 但不保存到磁盘——用临时路径传递，读取后删除
                    QString tmpPath = inputPath + ".ace_tmp.png";
                    cv::imwrite(tmpPath.toStdString(), result);
                    emit finished(tmpPath);
                } catch (const std::exception& e) {
                    emit errorOccurred(tr("Image translation error: ") + QString::fromUtf8(e.what()));
                }
                break;
            }
            }
        } catch (const std::exception& e) {
            emit errorOccurred(tr("Translation error: ") + QString::fromUtf8(e.what()));
        } catch (...) {
            emit errorOccurred(tr("Unknown error (DLL load failure)"));
        }
}

// ============================================================
// DropZoneWidget — 拖拽文件放置区域
// ============================================================
DropZoneWidget::DropZoneWidget(QWidget* parent) : QFrame(parent) {
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    setObjectName("fileDropZone");
    setMinimumHeight(120);
}

void DropZoneWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        setProperty("dragOver", true);
        style()->unpolish(this);
        style()->polish(this);
    }
}

void DropZoneWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void DropZoneWidget::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasUrls()) {
        QStringList paths;
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                paths.append(url.toLocalFile());
            }
        }
        if (!paths.isEmpty()) {
            emit fileDropped(paths);
        }
        event->acceptProposedAction();
    }
    setProperty("dragOver", false);
    style()->unpolish(this);
    style()->polish(this);
}

void DropZoneWidget::mousePressEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    // On click, emit fileDropped with empty list to trigger file dialog
    emit fileDropped(QStringList());
}

// ============================================================
// MainWindow
// ============================================================
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("AceTranslatePro"));
    // 用圆角PNG作为窗口图标
    {
        QPixmap src(":/icons/LOGO.png");
        if (!src.isNull()) {
            int s = qMin(src.width(), src.height());
            QPixmap rounded(s, s);
            rounded.fill(Qt::transparent);
            QPainter p(&rounded);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addRoundedRect(0, 0, s, s, s * 0.18, s * 0.18);
            p.setClipPath(path);
            p.drawPixmap(QRect(0, 0, s, s), src);
            p.end();
            setWindowIcon(QIcon(rounded));
        } else {
            setWindowIcon(QIcon(":/icons/LOGO.png"));
        }
    }
    // RegionCapture 截图选区组件
    regionCapture_ = new RegionCapture(this);
    connect(regionCapture_, &RegionCapture::regionCaptured, this, [this]() {
        onCaptureComplete();
    });

    // 从配置加载语言设置
    {
        auto& cfg = docmind::ConfigManager::getInstance();
        std::string lang = cfg.getNestedJson("defaults").value("ui_language", nlohmann::json("zh_CN")).get<std::string>();
        QString langCode = QString::fromStdString(lang);
        MainWindow::switchLanguage(langCode);
    }

    // FloatTranslateWindow 划词翻译悬浮窗（独立顶层窗口）
    floatWindow_ = new FloatTranslateWindow(nullptr);
    connect(floatWindow_, &FloatTranslateWindow::translationDone, this, [this](const QString& src, const QString& result) {
        if (floatHistoryView_) {
            QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
            floatHistoryView_->append(QStringLiteral("<small style='color:#889096;'>[%1]</small>").arg(ts));
            floatHistoryView_->append(QStringLiteral("<b style='color:#1A1A2E;'>%1</b>").arg(src.toHtmlEscaped()));
            floatHistoryView_->append(QStringLiteral("<span style='color:#0B7C72;'>%1</span><br>").arg(result.toHtmlEscaped()));
        }
    });

    setupUI();
    createTrayIcon();
    resize(1113, 620);
    setMinimumSize(800, 550);
    onNavButtonClicked(0);

    // 注册全局热键
    winId();
    qApp->installNativeEventFilter(this);
    registerGlobalHotkeys();
}

MainWindow::~MainWindow() {
    // 注销全局热键
    UnregisterHotKey((HWND)winId(), floatHotkeyId_);
    UnregisterHotKey((HWND)winId(), screenshotHotkeyId_);
    qApp->removeNativeEventFilter(this);

    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(3000);
    }
    if (trayIcon_) {
        trayIcon_->hide();
        delete trayMenu_;
    }
    delete floatWindow_;
}

// ============================================================
// resizeEvent — 窗口大小变化时自适应缩放预览图片
// ============================================================
void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    auto tryZoomLabel = [](ZoomableLabel* label, QScrollArea* area, const cv::Mat& mat) {
        if (mat.empty() || !label || !area) return;
        if (label->hasImage() && label->zoomLevel() != 1.0) return;
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
        QPixmap fullPix = QPixmap::fromImage(qimg);
        if (!fullPix.isNull()) {
            int maxW = area->viewport()->width();
            int maxH = area->viewport()->height();
            if (maxW > 50 && maxH > 50) {
                // 存全分辨率图用于缩放，但适配显示缩略图
                QPixmap displayPix = fullPix.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                label->setFullImage(fullPix, displayPix);
            }
        }
    };
    tryZoomLabel(qobject_cast<ZoomableLabel*>(photoPreview_), photoInputScroll_, photoOrigMat_);
    tryZoomLabel(qobject_cast<ZoomableLabel*>(photoOutputPreview_), photoOutputScroll_, photoOutputMat_);

    // 截图预览框和图片跟随窗口缩放
    if (!screenshotFullPix_.isNull() && screenshotScroll_) {
        auto* zl = qobject_cast<ZoomableLabel*>(screenshotPreview_);
        if (zl && zl->hasImage()) {
            int maxW = screenshotScroll_->viewport()->width();
            int maxH = screenshotScroll_->viewport()->height();
            maxW = qMax(50, maxW);
            maxH = qMax(50, maxH);
            QPixmap displayPix = screenshotFullPix_.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            zl->setFullImage(screenshotFullPix_, displayPix);
        }
    }
}

QTranslator s_translator;
static QString s_currentLang;

// ============================================================
// 语言切换（静态方法）
// ============================================================
void MainWindow::switchLanguage(const QString& langCode) {
    if (langCode == s_currentLang) return;
    s_currentLang = langCode;

    // 移除旧翻译
    qApp->removeTranslator(&s_translator);

    // zh_CN: 加载中文翻译 → 显示中文
    // en_US: 不加载翻译 → Qt 显示 tr() 中的英文源文本
    // ja_JP: 加载日文翻译 → 显示日文
    if (langCode != QStringLiteral("en_US")) {
        QString qmFile = QStringLiteral(":/translations/") + langCode + QStringLiteral(".qm");
        if (s_translator.load(qmFile)) {
            qApp->installTranslator(&s_translator);
        }
    }
}

// ============================================================
// 事件处理
// ============================================================

void MainWindow::closeEvent(QCloseEvent* event) {
    if (trayIcon_ && trayIcon_->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

// ============================================================
// eventFilter — 导航按钮悬浮放大 + 文本卡片悬浮动效
// ============================================================
bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    // ---- 阻止 QComboBox 鼠标滚轮改变选项（仍允许页面滚动） ----
    if (event->type() == QEvent::Wheel) {
        if (qobject_cast<QComboBox*>(obj) && qobject_cast<QComboBox*>(obj)->isVisible()) {
            // 向上查找 QScrollArea，传递滚轮事件到其 viewport
            QWidget* p = qobject_cast<QComboBox*>(obj)->parentWidget();
            while (p) {
                auto* sa = qobject_cast<QScrollArea*>(p);
                if (sa) {
                    QApplication::sendEvent(sa->viewport(), event);
                    break;
                }
                p = p->parentWidget();
            }
            return true;
        }
    }
    // ---- 导航按钮: 图标放大 ----
    auto* btn = qobject_cast<QPushButton*>(obj);
    if (btn && btn->objectName() == "navBtn") {
        if (event->type() == QEvent::Enter) {
            auto* anim = new QPropertyAnimation(btn, "iconSize");
            anim->setDuration(180);
            anim->setStartValue(btn->iconSize());
            anim->setEndValue(QSize(30, 30));
            anim->setEasingCurve(QEasingCurve::OutCubic);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else if (event->type() == QEvent::Leave) {
            auto* anim = new QPropertyAnimation(btn, "iconSize");
            anim->setDuration(180);
            anim->setStartValue(btn->iconSize());
            anim->setEndValue(QSize(24, 24));
            anim->setEasingCurve(QEasingCurve::OutCubic);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
        return QMainWindow::eventFilter(obj, event);
    }

    // ---- 各模块卡片: 悬浮放大 + 边框高亮 ----
    {
        // 收集所有卡片
        QFrame* card = nullptr;
        bool isSource = false, isResult = false;
        if (obj == textSourceCard_) { card = textSourceCard_; isSource = true; }
        else if (obj == textResultCard_) { card = textResultCard_; isResult = true; }
        else if (obj == screenshotPreviewCard_) { card = screenshotPreviewCard_; isSource = true; }
        else if (obj == screenshotResultCard_) { card = screenshotResultCard_; isResult = true; }
        else if (obj == photoInputCard_) { card = photoInputCard_; isSource = true; }
        else if (obj == photoOutputCard_) { card = photoOutputCard_; isResult = true; }
        else if (obj == floatConfigCard_) { card = floatConfigCard_; isSource = true; }
        else if (obj == historyCard_) { card = historyCard_; isSource = true; }

        if (card) {
            if (event->type() == QEvent::Enter) {
                // 细边框高亮 (1px)
                QString base = isResult
                    ? "QFrame { background: #F0F7F6; border: 1px solid #0B7C72; border-radius: 8px; }"
                    : "QFrame#card { background-color: #FFFFFF; border: 1px solid #0B7C72; border-radius: 8px; padding: 16px; }";
                card->setStyleSheet(base);

                // 阴影弹出效果
                auto* shadow = new QGraphicsDropShadowEffect;
                shadow->setBlurRadius(20);
                shadow->setOffset(0, 4);
                shadow->setColor(QColor(11, 124, 114, 50));
                card->setGraphicsEffect(shadow);

                // 微放大
                int grow = 6;
                card->setFixedHeight(card->height() + grow);

            } else if (event->type() == QEvent::Leave) {
                // 恢复原始边框
                if (isResult) {
                    card->setStyleSheet(
                        "QFrame { background: #F0F7F6; border: 1px solid #D0E8E4; border-radius: 8px; }");
                } else {
                    card->setStyleSheet(
                        "QFrame#card { background-color: #FFFFFF; border: 1px solid #E8ECEF;"
                        " border-radius: 8px; padding: 16px; }");
                }

                // 移除阴影
                card->setGraphicsEffect(nullptr);

                // 恢复大小
                card->setFixedHeight(QWIDGETSIZE_MAX);
            }
            return QMainWindow::eventFilter(obj, event);
        }
    }

    // ---- 文件列表项：双击打开文件 ----
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto* f = qobject_cast<QFrame*>(obj);
        if (f) {
            QString fp = f->property("filePath").toString();
            if (!fp.isEmpty() && QFile::exists(fp)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(fp));
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

// ============================================================
// setupUI — 顶栏 + 左导航 + 右内容区
// ============================================================
void MainWindow::setupUI() {
    setWindowTitle(QStringLiteral("AceTranslatePro"));

    // 中央容器: 垂直布局 (header + content split)
    auto* centralWidget = new QWidget(this);
    auto* mainContainer = new QVBoxLayout(centralWidget);
    mainContainer->setContentsMargins(0, 0, 0, 0);
    mainContainer->setSpacing(0);
    setCentralWidget(centralWidget);

    // ====== 顶部 Header 栏 ======
    auto* headerBar = new QFrame;
    headerBar->setObjectName("headerBar");
    headerBar->setFixedHeight(40);
    auto* headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(16, 0, 16, 0);

    auto* headerLogo = new QLabel;
    QPixmap logoPixmap(":/icons/LOGO.png");
    if (!logoPixmap.isNull()) {
        headerLogo->setPixmap(logoPixmap.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        headerLogo->setText(QStringLiteral("AT"));
        headerLogo->setStyleSheet(
            "font-size: 20px; font-weight: bold; color: #0D9488;"
            "background: #EEF1FF; padding: 4px 10px; border-radius: 8px;");
    }
    headerLogo->setFixedHeight(28);
    headerLayout->addWidget(headerLogo);

    auto* headerTitle = new QLabel(QStringLiteral("AceTranslatePro"));
    headerTitle->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: #134E4A; padding-left: 8px;");
    headerLayout->addWidget(headerTitle);

    headerLayout->addStretch();

    auto* settingsIconBtn = new QPushButton;
    settingsIconBtn->setFixedSize(36, 36);
    settingsIconBtn->setIcon(QIcon(":/icons/setting.png"));
    settingsIconBtn->setIconSize(QSize(24, 24));
    settingsIconBtn->setCursor(Qt::PointingHandCursor);
    settingsIconBtn->setStyleSheet(
        "QPushButton { border: none; border-radius: 18px; background: transparent; }"
        "QPushButton:hover { background: #F0FDFA; }");
    connect(settingsIconBtn, &QPushButton::clicked, this, [this]() {
        onNavButtonClicked(5);
    });
    headerLayout->addWidget(settingsIconBtn);

    mainContainer->addWidget(headerBar);

    // ====== 内容区: 水平分割 (导航 + 页面) ======
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(4);
    splitter->setChildrenCollapsible(false);

    // ====== 左侧导航面板 ======
    navPanel_ = new QFrame;
    navPanel_->setObjectName("navPanel");
    navPanel_->setMinimumWidth(60);
    navPanel_->setMaximumWidth(120);
    auto* navLayout = new QVBoxLayout(navPanel_);
    navLayout->setContentsMargins(0, 8, 4, 8);  // 左边距0，container竖杠紧贴边框
    navLayout->setSpacing(2);
    navLayout->setAlignment(Qt::AlignTop);

    // 前 5 个导航按钮 (Text / Float / Screenshot / Photo / File)
    struct NavDef { const char* icon; const char* label_cn; const char* label_en; };
    NavDef navDefs[] = {
        { ":/icons/text.png", "文本翻译", "Text Translation" },
        { ":/icons/selection.png", "划词翻译", "Selection Translation" },
        { ":/icons/screenshot.png", "截图翻译", "Screenshot Translation" },
        { ":/icons/image_.png", "图片翻译", "Image Translation" },
        { ":/icons/file.png", "文件翻译", "File Translation" },
    };
    for (int i = 0; i < 5; ++i) {
        navLayout->addWidget(createNavButton(i, QString::fromUtf8(navDefs[i].icon),
                                              tr(navDefs[i].label_en)));
    }

    // 弹簧
    navLayout->addStretch();

    // 分隔线
    auto* sep2 = new QFrame;
    sep2->setObjectName("separator");
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFixedHeight(1);
    navLayout->addWidget(sep2);
    navLayout->addSpacing(4);

    // 底部的设置按钮
    navLayout->addWidget(createNavButton(5, QStringLiteral(":/icons/setting.png"),
                                          tr("Settings")));

    splitter->addWidget(navPanel_);

    // ====== 右侧内容区 ======
    auto* contentFrame = new QFrame;
    contentFrame->setObjectName("card");
    contentFrame->setMinimumWidth(400);
    auto* contentLayout = new QVBoxLayout(contentFrame);
    contentLayout->setContentsMargins(20, 20, 20, 20);

    stackedWidget_ = new QStackedWidget;
    stackedWidget_->addWidget(createTextPanel());          // 0
    stackedWidget_->addWidget(createFloatConfigPanel());   // 1
    stackedWidget_->addWidget(createScreenshotPanel());    // 2
    stackedWidget_->addWidget(createPhotoPanel());         // 3
    stackedWidget_->addWidget(createFilePanel());          // 4
    stackedWidget_->addWidget(createSettingsPanel());      // 5

    contentLayout->addWidget(stackedWidget_);
    splitter->addWidget(contentFrame);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    mainContainer->addWidget(splitter);

    // 状态栏 + 进度条
    statusBar_ = statusBar();
    statusBar_->setStyleSheet(
        "QStatusBar { background: #F8FAFA; border-top: 1px solid #E8ECEF;"
        "  font-size: 12px; color: #889096; }"
    );

    // 把图标和文字放在一个容器里，避免 QStatusBar 画分隔线
    auto* statusContainer = new QWidget;
    auto* statusHBox = new QHBoxLayout(statusContainer);
    statusHBox->setContentsMargins(0, 0, 0, 0);
    statusHBox->setSpacing(2);
    statusIcon_ = new QLabel;
    QPixmap yesIcon(":/icons/yes.png");
    if (!yesIcon.isNull()) {
        statusIcon_->setPixmap(yesIcon.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        statusIcon_->setPixmap(createCheckIcon());
    }
    statusIcon_->setFixedSize(16, 16);
    statusIcon_->hide();
    statusHBox->addWidget(statusIcon_);
    statusLabel_ = new QLabel(tr("Ready"));
    statusLabel_->setStyleSheet("QLabel { border: none; margin: 0; padding: 0; font-size: 12px; color: #5B6269; }");
    statusHBox->addWidget(statusLabel_);
    statusHBox->addStretch();
    statusBar_->addWidget(statusContainer, 1);
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->setFixedWidth(200);
    progressBar_->hide();
    statusBar_->addPermanentWidget(progressBar_);

    // 沙漏动画定时器
    hourglassTimer_ = new QTimer(this);
    hourglassTimer_->setInterval(500);
    connect(hourglassTimer_, &QTimer::timeout, this, &MainWindow::onHourglassTick);

    // 阻止所有 QComboBox 鼠标滚轮改变选项
    for (auto* cb : findChildren<QComboBox*>())
        cb->installEventFilter(this);
}

// ============================================================
// createNavButton — 创建导航按钮 (icon + 下方文字)
// ============================================================
QWidget* MainWindow::createNavButton(int index, const QString& iconPath,
                                      const QString& label) {
    auto* container = new QWidget;
    container->setObjectName("navContainer");
    container->setMinimumWidth(60);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 6, 0, 6);
    layout->setSpacing(2);
    layout->setAlignment(Qt::AlignHCenter);

    auto* btn = new QPushButton;
    btn->setObjectName("navBtn");
    btn->setProperty("origIconSize", QSize(24, 24));
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(24, 24));
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(52, 52);
    btn->installEventFilter(this);

    // 确保 navButtons_ 有足够空间
    while (navButtons_.size() <= index)
        navButtons_.append(nullptr);
    navButtons_[index] = btn;

    connect(btn, &QPushButton::clicked, this, [this, index]() {
        onNavButtonClicked(index);
    });

    layout->addWidget(btn, 0, Qt::AlignHCenter);

    auto* lbl = new QLabel;
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setObjectName("navLabel");
    lbl->setText(label);
    lbl->setWordWrap(true);
    layout->addWidget(lbl, 0, Qt::AlignHCenter);

    return container;
}

// ============================================================
// onNavButtonClicked — 导航按钮点击处理
// ============================================================
void MainWindow::onNavButtonClicked(int index) {
    for (int i = 0; i < navButtons_.size(); ++i) {
        if (navButtons_[i])
            navButtons_[i]->setChecked(i == index);
    }
    // 更新 navContainer 的 selected 属性用于竖杠样式
    for (int i = 0; i < navButtons_.size(); ++i) {
        auto* container = navButtons_[i] ? navButtons_[i]->parentWidget() : nullptr;
        if (container) {
            bool sel = (i == index);
            container->setProperty("selected", sel);
            container->style()->unpolish(container);
            container->style()->polish(container);
            // 强制重绘
            container->update();
        }
    }
    currentNavIndex_ = index;
    stackedWidget_->setCurrentIndex(index);
}

// ============================================================
// Panel 创建方法
// ============================================================

// —————— 文本翻译面板 (双栏布局, 匹配 HTML ref) ——————
QWidget* MainWindow::createTextPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // ---- 标题 ----
    auto* title = new QLabel(tr("Text Translation"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(tr("Enter or paste text to translate"));
    hint->setObjectName("sectionHint");
    layout->addWidget(hint);

    // ---- 1. 语言条 ----
    auto* langBar = new QFrame;
    langBar->setObjectName("languageBar");
    auto* langLayout = new QHBoxLayout(langBar);
    langLayout->setContentsMargins(10, 6, 10, 6);
    langLayout->setSpacing(6);

    auto* sourceLabel = new QLabel(tr("Source Language:"));
    sourceLabel->setStyleSheet("font-size: 12px; color: #889096;");
    langLayout->addWidget(sourceLabel);

    auto* autoDetectLabel = new QLabel(tr("Auto Detect"));
    autoDetectLabel->setStyleSheet(
        "font-size: 12px; font-weight: 500; color: #0B7C72;"
        "background: #E8F5F3; border: 1px solid #D0E8E4; border-radius: 10px;"
        " padding: 4px 10px;");
    langLayout->addWidget(autoDetectLabel);

    // 垂直分隔线
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color: #E5E5EA; padding: 0 6px;");
    langLayout->addWidget(sep);

    auto* targetLabel = new QLabel(tr("Target Language:"));
    targetLabel->setStyleSheet("font-size: 12px; color: #889096;");
    langLayout->addWidget(targetLabel);

    textLangCombo_ = new QComboBox;
    populateLanguages(textLangCombo_, QString(), true);
    textLangCombo_->setFixedHeight(30);
    textLangCombo_->setMinimumWidth(120);
    textLangCombo_->setStyleSheet(
        "QComboBox { border: 1px solid #DDE1E5; border-radius: 6px;"
        " padding: 4px 8px; font-size: 13px; background: white; }");
    langLayout->addWidget(textLangCombo_);

    langLayout->addStretch();

    auto* langHint = new QLabel;
    langHint->setStyleSheet("font-size: 12px; color: #889096;");
    langLayout->addWidget(langHint);

    layout->addWidget(langBar);

    // ---- 2. 翻译双栏 ----
    auto* gridLayout = new QHBoxLayout;
    gridLayout->setSpacing(16);

    // ---- 左栏: 原文 ----
    auto* leftPanel = new QFrame;
    leftPanel->setObjectName("card");
    textSourceCard_ = leftPanel;
    leftPanel->installEventFilter(this);
    auto* leftInner = new QVBoxLayout(leftPanel);
    leftInner->setContentsMargins(14, 10, 14, 10);
    leftInner->setSpacing(6);

    // 原文标题行
    auto* leftHeaderRow = new QHBoxLayout;
    leftHeaderRow->setContentsMargins(0, 0, 0, 0);
    leftHeaderRow->setSpacing(4);
    auto* leftIcon = new QLabel;
    QPixmap orgPix(":/icons/org_image.png");
    if (!orgPix.isNull())
        leftIcon->setPixmap(orgPix.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    leftHeaderRow->addWidget(leftIcon);
    auto* leftTitle = new QLabel(tr("Source Text"));
    leftTitle->setStyleSheet("font-size: 12px; font-weight: 600; color: #889096; text-transform: uppercase;");
    leftHeaderRow->addWidget(leftTitle);
    leftHeaderRow->addStretch();

    textPasteBtn_ = new QPushButton(tr("Paste"));
    textPasteBtn_->setCursor(Qt::PointingHandCursor);
    textPasteBtn_->setStyleSheet(
        "QPushButton { border: none; background: transparent; color: #889096; font-size: 12px; padding: 0px 4px; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(11, 124, 114, 0.08); color: #0B7C72; }");
    connect(textPasteBtn_, &QPushButton::clicked, this, [this]() {
        if (textInput_) textInput_->insertPlainText(QApplication::clipboard()->text());
    });
    leftHeaderRow->addWidget(textPasteBtn_);

    textClearBtn_ = new QPushButton(tr("Clear"));
    textClearBtn_->setCursor(Qt::PointingHandCursor);
    textClearBtn_->setStyleSheet(textPasteBtn_->styleSheet());
    connect(textClearBtn_, &QPushButton::clicked, this, [this]() {
        if (textInput_) { textInput_->clear(); }
        if (textOutput_) { textOutput_->clear(); }
    });
    leftHeaderRow->addWidget(textClearBtn_);

    leftInner->addLayout(leftHeaderRow);

    // 用容器包裹输入框 + footer（避免 footer 影响标题行间距）
    auto* inputContainer = new QWidget;
    inputContainer->setStyleSheet("background: transparent; border: none;");
    auto* inputLay = new QVBoxLayout(inputContainer);
    inputLay->setContentsMargins(0, 0, 0, 0);
    inputLay->setSpacing(0);

    textInput_ = new QPlainTextEdit;
    textInput_->setPlaceholderText(tr("Enter text to translate…"));
    textInput_->setMaximumHeight(120);
    textInput_->setStyleSheet(
        "QPlainTextEdit { border: none; background: transparent; font-size: 14px;"
        " color: #1A1A2E; padding: 2px 0; }"
        "QPlainTextEdit:focus { border: none; }");
    inputLay->addWidget(textInput_, 1);

    // 底部条：0 字符 + 麦克风
    auto* leftFooterRow = new QHBoxLayout;
    leftFooterRow->setContentsMargins(0, 0, 0, 0);
    auto* leftFooter = new QLabel(tr("0 characters"));
    leftFooter->setStyleSheet("font-size: 11px; color: #C4C8CC;");
    leftFooterRow->addWidget(leftFooter);
    leftFooterRow->addStretch();

    textMicBtn_ = new QPushButton;
    textMicBtn_->setFocusPolicy(Qt::NoFocus);
    textMicBtn_->setFlat(true);
    QPixmap voicePix(":/icons/voice.png");
    if (!voicePix.isNull()) {
        textMicBtn_->setIcon(QIcon(voicePix.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        textMicBtn_->setIconSize(QSize(16, 16));
    } else {
        textMicBtn_->setText(QStringLiteral("🎤"));
    }
    textMicBtn_->setCursor(Qt::PointingHandCursor);
    textMicBtn_->setToolTip(QString());
    textMicBtn_->setFixedSize(20, 14);
    textMicBtn_->setStyleSheet(
        "QPushButton { border: 0px; background: transparent; padding: 0px 2px; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(11, 124, 114, 0.12); }"
        "QPushButton:focus { outline: none; }");
    connect(textMicBtn_, &QPushButton::clicked, this, [this]() {
        onMicButtonClicked();
    });
    leftFooterRow->addWidget(textMicBtn_);
    inputLay->addLayout(leftFooterRow);

    leftInner->addWidget(inputContainer, 1);

    connect(textInput_, &QPlainTextEdit::textChanged, this, [leftFooter, this]() {
        leftFooter->setText(tr("%1 characters").arg(textInput_->toPlainText().length()));
    });

    gridLayout->addWidget(leftPanel, 1);

    // ---- 右栏: 译文 ----
    auto* rightPanel = new QFrame;
    textResultCard_ = rightPanel;
    rightPanel->installEventFilter(this);
    rightPanel->setStyleSheet(
        "QFrame { background: #F0F7F6; border: 1px solid #D0E8E4; border-radius: 8px; }");
    auto* rightInner = new QVBoxLayout(rightPanel);
    rightInner->setContentsMargins(14, 28, 14, 10);
    rightInner->setSpacing(4);

    auto* rightHeaderRow = new QHBoxLayout;
    rightHeaderRow->setContentsMargins(0, 0, 0, 0);
    rightHeaderRow->setSpacing(4);
    auto* rightIcon = new QLabel;
    rightIcon->setFixedSize(18, 18);
    rightIcon->setStyleSheet("background: transparent; border: none; padding: 0; margin: 0;");
    QPixmap transPix(":/icons/trans.png");
    if (!transPix.isNull())
        rightIcon->setPixmap(transPix.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    rightHeaderRow->addWidget(rightIcon);
    auto* rightTitle = new QLabel(tr("Translation Result"));
    rightTitle->setStyleSheet("font-size: 12px; font-weight: 600; color: #0B7C72; text-transform: uppercase; background: transparent; border: none; padding: 0; margin: 0;");
    rightHeaderRow->addWidget(rightTitle);
    rightHeaderRow->addStretch();

    textCopyBtn_ = new QPushButton(tr("Copy"));
    textCopyBtn_->setCursor(Qt::PointingHandCursor);
    textCopyBtn_->setStyleSheet(
        "QPushButton { border: none; background: transparent; color: #0B7C72; font-size: 12px; padding: 2px 6px; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(11, 124, 114, 0.12); }");
    connect(textCopyBtn_, &QPushButton::clicked, this, [this]() {
        if (textOutput_ && !textOutput_->toPlainText().isEmpty()) {
            QApplication::clipboard()->setText(textOutput_->toPlainText());
            statusBar_->showMessage(tr("Copied"), 2000);
        }
    });
    rightHeaderRow->addWidget(textCopyBtn_);

    textSpeakBtn_ = new QPushButton(tr("Read Aloud"));
    textSpeakBtn_->setCursor(Qt::PointingHandCursor);
    textSpeakBtn_->setStyleSheet(textCopyBtn_->styleSheet());
    connect(textSpeakBtn_, &QPushButton::clicked, this, [this]() {
        speakText(textOutput_ ? textOutput_->toPlainText() : QString());
    });
    rightHeaderRow->addWidget(textSpeakBtn_);

    rightInner->addLayout(rightHeaderRow);

    // 用容器包裹输出框（与原文侧 inputContainer 对称，保持高度一致）
    auto* rightContainer = new QWidget;
    rightContainer->setStyleSheet("background: transparent; border: none;");
    auto* rightLay = new QVBoxLayout(rightContainer);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);

    textOutput_ = new QPlainTextEdit;
    textOutput_->setReadOnly(true);
    textOutput_->setMaximumHeight(120);
    textOutput_->setPlaceholderText(tr("Translation Result…"));
    textOutput_->setStyleSheet(
        "QPlainTextEdit { border: none; background: transparent; font-size: 14px;"
        " color: #1A1A2E; padding: 2px 0; }");
    rightLay->addWidget(textOutput_, 1);

    // 占位 footer（与原文的 0 字符+麦克风行等高）
    auto* rightFooterRow = new QHBoxLayout;
    rightFooterRow->setContentsMargins(0, 0, 0, 0);
    rightFooterRow->setSpacing(0);
    auto* rightFooter = new QLabel;
    rightFooter->setFixedHeight(14);
    rightFooterRow->addWidget(rightFooter);
    rightLay->addLayout(rightFooterRow);

    rightInner->addWidget(rightContainer, 1);

    gridLayout->addWidget(rightPanel, 1);

    layout->addLayout(gridLayout);

    // ---- 3. 底部操作行 ----
    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);

    auto* quickFileBtn = new QPushButton(tr("Upload File"));
    quickFileBtn->setObjectName("pillBtn");
    connect(quickFileBtn, &QPushButton::clicked, this, [this]() { onNavButtonClicked(4); });
    bottomRow->addWidget(quickFileBtn);

    auto* quickImageBtn = new QPushButton(tr("Image Translation"));
    quickImageBtn->setObjectName("pillBtn");
    connect(quickImageBtn, &QPushButton::clicked, this, [this]() { onNavButtonClicked(3); });
    bottomRow->addWidget(quickImageBtn);

    bottomRow->addStretch();

    auto* translateBtn = new QPushButton(tr("Translate"));
    translateBtn->setObjectName("primaryBtn");
    textTranslateBtn_ = translateBtn;
    connect(translateBtn, &QPushButton::clicked, this, &MainWindow::onTranslateText);
    bottomRow->addWidget(translateBtn);

    layout->addLayout(bottomRow);

    return panel;
}

// —————— 浮动窗口配置面板 ——————
QWidget* MainWindow::createFloatConfigPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("Selection Translation"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(tr("Select text in any app, press hotkey to translate"));
    hint->setObjectName("sectionHint");
    layout->addWidget(hint);

    // ====== 配置卡片 ======
    auto* configCard = new QFrame;
    configCard->setObjectName("card");
    floatConfigCard_ = configCard;
    configCard->installEventFilter(this);
    auto* cardLayout = new QVBoxLayout(configCard);
    cardLayout->setSpacing(12);

    // 1) 快捷键行
    auto* hotkeyRow = new QHBoxLayout;
    auto* hotkeyLabel = new QLabel(tr("Hotkey"));
    hotkeyLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #889096;");
    hotkeyRow->addWidget(hotkeyLabel);
    hotkeyRow->addSpacing(12);

    floatHotkeyBtn_ = new QPushButton(QStringLiteral("Ctrl+Shift+C"));
    floatHotkeyBtn_->setObjectName("hotkeyBtn");
    floatHotkeyBtn_->setCursor(Qt::PointingHandCursor);
    // hotkey click dialog (same as before)
    connect(floatHotkeyBtn_, &QPushButton::clicked, this, [this]() {
        // 打开对话框前先注销快捷键，防止 QKeySequenceEdit 捕获按键时触发 WM_HOTKEY
        UnregisterHotKey((HWND)winId(), floatHotkeyId_);

        // 获取当前快捷键
        QString currentText = floatHotkeyBtn_->text();

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle(tr("Set Selection Hotkey"));
        dialog->setFixedSize(380, 240);
        dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dialog->setAttribute(Qt::WA_TranslucentBackground);
        dialog->setStyleSheet(QStringLiteral(
            "QDialog { background: #FFFFFF; border-radius: 14px; }"
        ));

        // 圆角背景容器
        auto* container = new QWidget(dialog);
        container->setStyleSheet(QStringLiteral(
            "QWidget { background: #FFFFFF; border-radius: 14px; }"
        ));
        auto* dlgLayout = new QVBoxLayout(container);
        dlgLayout->setContentsMargins(24, 22, 24, 18);
        dlgLayout->setSpacing(0);

        // 标题
        auto* title = new QLabel(tr("Set Selection Hotkey"));
        title->setStyleSheet(QStringLiteral(
            "font-size: 16px; font-weight: 600; color: #1C1C1E; background: transparent;"
        ));
        dlgLayout->addWidget(title);

        dlgLayout->addSpacing(8);

        // 说明
        auto* hint = new QLabel(tr("Press new hotkey (Ctrl or Alt required)"));
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: #8E8E93; background: transparent;"
        ));
        dlgLayout->addWidget(hint);

        dlgLayout->addSpacing(16);

        // 输入框
        auto* editor = new QKeySequenceEdit(container);
        editor->setStyleSheet(QStringLiteral(
            "QKeySequenceEdit {"
            "  border: 1px solid #D1D1D6; border-radius: 8px;"
            "  padding: 10px; font-size: 15px; background: #F8F9FA;"
            "  min-height: 22px;"
            "}"
            "QKeySequenceEdit:focus { border-color: #007AFF; background: #FFFFFF; }"
        ));
        dlgLayout->addWidget(editor);

        dlgLayout->addSpacing(18);

        // 按钮行
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);

        auto* cancelBtn = new QPushButton(tr("Cancel"), container);
        cancelBtn->setFixedHeight(36);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #F2F2F7; color: #007AFF; font-size: 13px;"
            "  border: none; border-radius: 8px; padding: 0 24px; }"
            "QPushButton:hover { background: #E5E5EA; }"
        ));

        auto* okBtn = new QPushButton(tr("OK"), container);
        okBtn->setFixedHeight(36);
        okBtn->setCursor(Qt::PointingHandCursor);
        okBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #007AFF; color: #FFFFFF; font-size: 13px;"
            "  border: none; border-radius: 8px; padding: 0 24px; }"
            "QPushButton:hover { background: #0056CC; }"
        ));

        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(okBtn);
        dlgLayout->addLayout(btnRow);

        // 主布局
        auto* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->addWidget(container);

        connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
        connect(okBtn, &QPushButton::clicked, dialog, [dialog, editor, currentText, this]() {
            QKeySequence ks = editor->keySequence();
            if (ks.isEmpty()) return;

            QString newText = ks.toString();
            // 检查是否与当前快捷键相同
            if (newText == currentText) {
                dialog->reject();
                return;
            }

            int key = ks[0].key();
            int mods = MOD_NOREPEAT;
            Qt::KeyboardModifiers qMods = ks[0].keyboardModifiers();
            if (qMods & Qt::ControlModifier) mods |= MOD_CONTROL;
            if (qMods & Qt::ShiftModifier) mods |= MOD_SHIFT;
            if (qMods & Qt::AltModifier) mods |= MOD_ALT;
            key = key & ~(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::META);

            if (mods == MOD_NOREPEAT) {
                QMessageBox::warning(dialog, tr("Hotkey"),
                    QStringLiteral("请至少包含 Ctrl 或 Alt 修饰键。\n例如：Ctrl+Shift+D"));
                return;
            }

            // 检查是否与截图快捷键冲突
            if (key == screenshotHotkeyKey_ && mods == screenshotHotkeyMods_) {
                QMessageBox::warning(dialog, tr("Hotkey Conflict"),
                    tr("This hotkey is used by Screenshot Translation."));
                return;
            }

            // 保存并注册
            {
                auto& c = docmind::ConfigManager::getInstance();
                c.setString("hotkey.float.key", std::to_string(key));
                c.setString("hotkey.float.mods", std::to_string(mods));
                c.save();
            }
            floatHotkeyKey_ = key;
            floatHotkeyMods_ = mods;
            UnregisterHotKey((HWND)winId(), floatHotkeyId_);
            BOOL ok = RegisterHotKey((HWND)winId(), floatHotkeyId_, floatHotkeyMods_, floatHotkeyKey_);
            if (!ok) {
                DWORD err = GetLastError();
                floatHotkeyKey_ = 'C';
                floatHotkeyMods_ = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
                floatHotkeyBtn_->setText(QStringLiteral("Ctrl+Shift+C"));
                RegisterHotKey((HWND)winId(), floatHotkeyId_, floatHotkeyMods_, floatHotkeyKey_);
                QMessageBox::warning(dialog, tr("Hotkey Registration Failed"),
                    QStringLiteral("热键 %1 注册失败（错误码: %2），可能被其他程序占用。\n已恢复为默认 Ctrl+Shift+C。")
                    .arg(newText).arg(err));
                return;
            }
            floatHotkeyBtn_->setText(newText);
            dialog->accept();
        });

        dialog->exec();
        // 对话框关闭后重新注册快捷键
        RegisterHotKey((HWND)winId(), floatHotkeyId_, floatHotkeyMods_, floatHotkeyKey_);
        dialog->deleteLater();
    });
    hotkeyRow->addWidget(floatHotkeyBtn_);
    hotkeyRow->addStretch();
    cardLayout->addLayout(hotkeyRow);

    // 2) 分隔线
    auto* sep3 = new QFrame;
    sep3->setFrameShape(QFrame::HLine);
    sep3->setFixedHeight(1);
    sep3->setStyleSheet("color: #E5E5EA;");
    cardLayout->addWidget(sep3);

    // 3) 语言 + 选项行
    auto* optionsRow = new QHBoxLayout;
    optionsRow->setSpacing(20);

    auto* langLabel = new QLabel(tr("Default Language"));
    langLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #889096;");
    optionsRow->addWidget(langLabel);

    auto* langCombo = new QComboBox;
    populateLanguages(langCombo, QString(), true);
    langCombo->setFixedWidth(160);
    optionsRow->addWidget(langCombo);

    optionsRow->addSpacing(20);

    auto* autoCopy = new QCheckBox(tr("Auto Copy"));
    autoCopy->setChecked(true);
    autoCopy->setObjectName("toggleSwitch");
    optionsRow->addWidget(autoCopy);

    auto* stayOnTop = new QCheckBox(tr("Stay on Top"));
    stayOnTop->setChecked(true);
    stayOnTop->setObjectName("toggleSwitch");
    optionsRow->addWidget(stayOnTop);

    optionsRow->addStretch();
    cardLayout->addLayout(optionsRow);

    layout->addWidget(configCard);

    // ====== 翻译历史卡片 ======
    auto* historyCard = new QFrame;
    historyCard->setObjectName("card");
    historyCard_ = historyCard;
    historyCard->installEventFilter(this);
    auto* histLayout = new QVBoxLayout(historyCard);
    auto* histTitle = new QLabel(tr("Translation History"));
    histTitle->setStyleSheet("font-size: 12px; font-weight: 600; color: #889096; text-transform: uppercase;");

    auto* histHeaderRow = new QHBoxLayout;
    histHeaderRow->addWidget(histTitle);
    histHeaderRow->addStretch();
    auto* clearHistBtn = new QPushButton(tr("Clear"));
    clearHistBtn->setFixedSize(40, 22);
    clearHistBtn->setCursor(Qt::PointingHandCursor);
    clearHistBtn->setStyleSheet(
        "QPushButton { font-size: 11px; color: #889096; background: transparent; border: 1px solid #DDE1E5; border-radius: 4px; }"
        "QPushButton:hover { color: #0B7C72; border-color: #0B7C72; }");
    connect(clearHistBtn, &QPushButton::clicked, this, [this]() {
        if (floatHistoryView_) floatHistoryView_->clear();
    });
    histHeaderRow->addWidget(clearHistBtn);
    histLayout->addLayout(histHeaderRow);
    floatHistoryView_ = new QTextEdit;
    floatHistoryView_->setReadOnly(true);
    floatHistoryView_->setPlaceholderText(tr("Translation history will appear here…"));
    floatHistoryView_->setMaximumHeight(160);
    histLayout->addWidget(floatHistoryView_);
    layout->addWidget(historyCard);

    layout->addStretch();
    return panel;
}

// —————— 截图翻译面板 ——————
QWidget* MainWindow::createScreenshotPanel() {
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    auto* panel = new QWidget;
    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    auto* shotTitle = new QLabel(tr("Screenshot Translation"));
    shotTitle->setObjectName("sectionTitle");
    lay->addWidget(shotTitle);

    auto* shotHint = new QLabel(tr("Capture screen area for OCR and translation"));
    shotHint->setObjectName("sectionHint");
    shotHint->setWordWrap(true);
    lay->addWidget(shotHint);

    // 操作行
    auto* actionRow = new QHBoxLayout;
    screenshotCaptureBtn_ = new QPushButton(tr("Screenshot Translation"));
    screenshotCaptureBtn_->setObjectName("primaryBtn");
    connect(screenshotCaptureBtn_, &QPushButton::clicked, this, [this]() {
        hide();
        regionCapture_->startCapture();
    });
    actionRow->addWidget(screenshotCaptureBtn_);

    // 可配置的快捷键按钮
    screenshotHotkeyBtn_ = new QPushButton(QStringLiteral("Ctrl+Shift+Z"));
    screenshotHotkeyBtn_->setObjectName("hotkeyBtn");
    screenshotHotkeyBtn_->setCursor(Qt::PointingHandCursor);
    screenshotHotkeyBtn_->setMaximumWidth(160);
    connect(screenshotHotkeyBtn_, &QPushButton::clicked, this, [this]() {
        // 打开对话框前先注销快捷键，防止 QKeySequenceEdit 捕获按键时触发 WM_HOTKEY
        UnregisterHotKey((HWND)winId(), screenshotHotkeyId_);

        QString currentText = screenshotHotkeyBtn_->text();

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle(tr("Set Screenshot Hotkey"));
        dialog->setFixedSize(380, 240);
        dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dialog->setAttribute(Qt::WA_TranslucentBackground);

        auto* container = new QWidget(dialog);
        container->setStyleSheet(QStringLiteral(
            "QWidget { background: #FFFFFF; border-radius: 14px; }"
        ));
        auto* dlgLayout = new QVBoxLayout(container);
        dlgLayout->setContentsMargins(24, 22, 24, 18);
        dlgLayout->setSpacing(0);

        auto* title = new QLabel(tr("Set Screenshot Hotkey"));
        title->setStyleSheet(QStringLiteral(
            "font-size: 16px; font-weight: 600; color: #1C1C1E; background: transparent;"
        ));
        dlgLayout->addWidget(title);
        dlgLayout->addSpacing(8);

        auto* hint = new QLabel(tr("Press new hotkey (Ctrl or Alt required)"));
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: #8E8E93; background: transparent;"
        ));
        dlgLayout->addWidget(hint);
        dlgLayout->addSpacing(16);

        auto* editor = new QKeySequenceEdit(container);
        editor->setStyleSheet(QStringLiteral(
            "QKeySequenceEdit {"
            "  border: 1px solid #D1D1D6; border-radius: 8px;"
            "  padding: 10px; font-size: 15px; background: #F8F9FA;"
            "  min-height: 22px;"
            "}"
            "QKeySequenceEdit:focus { border-color: #007AFF; background: #FFFFFF; }"
        ));
        dlgLayout->addWidget(editor);
        dlgLayout->addSpacing(18);

        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);

        auto* cancelBtn = new QPushButton(tr("Cancel"), container);
        cancelBtn->setFixedHeight(36);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #F2F2F7; color: #007AFF; font-size: 13px;"
            "  border: none; border-radius: 8px; padding: 0 24px; }"
            "QPushButton:hover { background: #E5E5EA; }"
        ));

        auto* okBtn = new QPushButton(tr("OK"), container);
        okBtn->setFixedHeight(36);
        okBtn->setCursor(Qt::PointingHandCursor);
        okBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #007AFF; color: #FFFFFF; font-size: 13px;"
            "  border: none; border-radius: 8px; padding: 0 24px; }"
            "QPushButton:hover { background: #0056CC; }"
        ));

        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(okBtn);
        dlgLayout->addLayout(btnRow);

        auto* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->addWidget(container);

        connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
        connect(okBtn, &QPushButton::clicked, dialog, [dialog, editor, currentText, this]() {
            QKeySequence ks = editor->keySequence();
            if (ks.isEmpty()) return;

            QString newText = ks.toString();
            if (newText == currentText) {
                dialog->reject();
                return;
            }

            int key = ks[0].key();
            int mods = MOD_NOREPEAT;
            if (key & Qt::CTRL) mods |= MOD_CONTROL;
            if (key & Qt::SHIFT) mods |= MOD_SHIFT;
            if (key & Qt::ALT) mods |= MOD_ALT;
            key = key & ~(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::META);

            // 检查是否与划词快捷键冲突
            if (key == floatHotkeyKey_ && mods == floatHotkeyMods_) {
                QMessageBox::warning(dialog, tr("Hotkey Conflict"),
                    tr("This hotkey is used by Selection Translation."));
                return;
            }

            screenshotHotkeyKey_ = key;
            screenshotHotkeyMods_ = mods;
            UnregisterHotKey((HWND)winId(), screenshotHotkeyId_);
            BOOL ok = RegisterHotKey((HWND)winId(), screenshotHotkeyId_, screenshotHotkeyMods_, screenshotHotkeyKey_);
            if (!ok) {
                qWarning("Screenshot hotkey re-register failed (error %lu)", GetLastError());
                screenshotHotkeyKey_ = 'Z';
                screenshotHotkeyMods_ = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
                screenshotHotkeyBtn_->setText(QStringLiteral("Ctrl+Shift+Z"));
                RegisterHotKey((HWND)winId(), screenshotHotkeyId_, screenshotHotkeyMods_, screenshotHotkeyKey_);
                QMessageBox::warning(dialog, tr("Hotkey Registration Failed"),
                    QStringLiteral("热键 %1 注册失败（错误码: %2），可能被其他程序占用。\n已恢复为默认 Ctrl+Shift+Z。")
                    .arg(newText).arg(GetLastError()));
                return;
            }
            screenshotHotkeyBtn_->setText(newText);
            dialog->accept();
        });

        dialog->exec();
        // 对话框关闭后重新注册快捷键
        RegisterHotKey((HWND)winId(), screenshotHotkeyId_, screenshotHotkeyMods_, screenshotHotkeyKey_);
        dialog->deleteLater();
    });
    actionRow->addWidget(screenshotHotkeyBtn_);
    actionRow->addStretch();
    lay->addLayout(actionRow);

    // 左右内容 — 对等大小
    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);

    // 左: 预览
    auto* previewCard = new QFrame;
    previewCard->setObjectName("card");
    screenshotPreviewCard_ = previewCard;
    previewCard->installEventFilter(this);
    auto* previewInner = new QVBoxLayout(previewCard);
    previewInner->setContentsMargins(12, 8, 12, 8);
    previewInner->setSpacing(6);
    auto* previewHeader = new QHBoxLayout;
    previewHeader->setSpacing(4);
    auto* previewIcon = new QLabel;
    previewIcon->setFixedSize(16, 16);
    QPixmap previewPix(QStringLiteral(":/icons/org_image.png"));
    if (!previewPix.isNull())
        previewIcon->setPixmap(previewPix.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    previewHeader->addWidget(previewIcon);
    auto* previewLabel = new QLabel(tr("Screenshot Preview"));
    previewLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #889096;");
    previewLabel->setFixedHeight(16);
    previewHeader->addWidget(previewLabel);
    previewHeader->addStretch();

    // 清空预览按钮
    auto* screenshotClearPreviewBtn = new QPushButton(tr("Clear"));
    screenshotClearPreviewBtn->setCursor(Qt::PointingHandCursor);
    screenshotClearPreviewBtn->setStyleSheet(
        "QPushButton { border: none; background: transparent; color: #889096; font-size: 12px; padding: 2px 6px; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(11, 124, 114, 0.08); color: #0B7C72; }");
    connect(screenshotClearPreviewBtn, &QPushButton::clicked, this, [this]() {
        screenshotPreview_->clearImage();
        screenshotPreview_->setFixedSize(360, 240);
        screenshotPreview_->setText(tr("Waiting for screenshot…"));
        screenshotFullPix_ = QPixmap();
        if (screenshotOcrResult_) screenshotOcrResult_->clear();
        if (screenshotResult_) screenshotResult_->clear();
        // 恢复默认大小后取消固定，让布局重新控制
        QTimer::singleShot(0, this, [this]() {
            screenshotPreview_->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            screenshotPreview_->setMinimumSize(200, 140);
        });
    });
    previewHeader->addWidget(screenshotClearPreviewBtn);
    previewInner->addLayout(previewHeader);
    screenshotPreview_ = new ZoomableLabel;
    screenshotPreview_->setMinimumSize(200, 140);
    screenshotPreview_->setAlignment(Qt::AlignCenter);
    screenshotPreview_->setStyleSheet("background: #F5F7F7; border-radius:4px; color:#86868B; font-size: 13px;");
    screenshotPreview_->setText(tr("Waiting for screenshot…"));
    auto* previewScroll = new QScrollArea;
    screenshotScroll_ = previewScroll;
    previewScroll->setWidget(screenshotPreview_);
    previewScroll->setWidgetResizable(true);
    previewScroll->setMinimumHeight(160);
    previewScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    previewScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    previewScroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    previewInner->addWidget(previewScroll, 1);
    contentRow->addWidget(previewCard, 1);

    // 右: 结果
    auto* resultCard = new QFrame;
    screenshotResultCard_ = resultCard;
    resultCard->installEventFilter(this);
    resultCard->setStyleSheet(
        "QFrame { background: #F0F7F6; border: 1px solid #D0E8E4; border-radius: 8px; }");
    auto* resultInner = new QVBoxLayout(resultCard);
    resultInner->setContentsMargins(14, 10, 14, 10);
    resultInner->setSpacing(6);

    // 识别结果子框（带图标）
    auto* ocrHeader = new QHBoxLayout;
    ocrHeader->setSpacing(6);
    auto* ocrIcon = new QLabel;
    ocrIcon->setFixedSize(18, 18);
    ocrIcon->setStyleSheet("background: transparent; border: none; padding: 0; margin: 0;");
    QPixmap ocrPix(QStringLiteral(":/icons/OCR.png"));
    if (!ocrPix.isNull())
        ocrIcon->setPixmap(ocrPix.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ocrHeader->addWidget(ocrIcon);
    auto* ocrLabel = new QLabel(tr("Recognition Result"));
    ocrLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #889096; background: transparent; border: none; padding: 0; margin: 0;");
    ocrHeader->addWidget(ocrLabel);
    ocrHeader->addStretch();
    resultInner->addLayout(ocrHeader);
    screenshotOcrResult_ = new QTextEdit;
    screenshotOcrResult_->setReadOnly(true);
    screenshotOcrResult_->setPlaceholderText(tr("OCR Text…"));
    screenshotOcrResult_->setStyleSheet("QTextEdit { border: 1px solid #E8ECEF; border-radius: 6px; background: transparent; font-size: 12px; color: #374151; padding: 4px; }");
    resultInner->addWidget(screenshotOcrResult_, 1);

    // 翻译结果子框（带图标）
    auto* transHeader = new QHBoxLayout;
    transHeader->setSpacing(6);
    auto* transIcon = new QLabel;
    transIcon->setFixedSize(18, 18);
    transIcon->setStyleSheet("background: transparent; border: none; padding: 0; margin: 0;");
    QPixmap transPix2(QStringLiteral(":/icons/trans.png"));
    if (!transPix2.isNull())
        transIcon->setPixmap(transPix2.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    transHeader->addWidget(transIcon);
    auto* transLabel = new QLabel(tr("Translation Result"));
    transLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #0B7C72; background: transparent; border: none; padding: 0; margin: 0;");
    transHeader->addWidget(transLabel);
    transHeader->addStretch();
    resultInner->addLayout(transHeader);

    // 翻译结果框 + 按钮行（放在带边框的容器中，按钮看起来在框内）
    auto* transContainer = new QVBoxLayout;
    transContainer->setSpacing(4);
    auto* transFrame = new QFrame;
    transFrame->setStyleSheet("QFrame { border: 1px solid #D0E8E4; border-radius: 6px; background: transparent; }");
    auto* transFrameLayout = new QVBoxLayout(transFrame);
    transFrameLayout->setContentsMargins(0, 0, 0, 0);
    transFrameLayout->setSpacing(0);
    screenshotResult_ = new QTextEdit;
    screenshotResult_->setReadOnly(true);
    screenshotResult_->setPlaceholderText(tr("Translation Result…"));
    screenshotResult_->setStyleSheet("QTextEdit { border: none; background: transparent; font-size: 13px; color: #1C1C1E; padding: 6px; }");
    transFrameLayout->addWidget(screenshotResult_, 1);

    // 复制 + 朗读按钮在框内右下
    auto* resultBtnRow = new QHBoxLayout;
    resultBtnRow->setContentsMargins(4, 0, 4, 2);
    resultBtnRow->setSpacing(4);
    resultBtnRow->addStretch();
    auto* screenshotCopyBtn = new QPushButton(tr("Copy"));
    screenshotCopyBtn->setCursor(Qt::PointingHandCursor);
    screenshotCopyBtn->setStyleSheet(
        "QPushButton { border: none; background: transparent; color: #0B7C72; font-size: 12px; padding: 2px 6px; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(11, 124, 114, 0.12); }");
    connect(screenshotCopyBtn, &QPushButton::clicked, this, [this]() {
        if (screenshotResult_ && !screenshotResult_->toPlainText().isEmpty()) {
            QApplication::clipboard()->setText(screenshotResult_->toPlainText());
            statusBar_->showMessage(tr("Copied"), 2000);
        }
    });
    resultBtnRow->addWidget(screenshotCopyBtn);
    auto* screenshotSpeakBtn = new QPushButton(tr("Read Aloud"));
    screenshotSpeakBtn->setCursor(Qt::PointingHandCursor);
    screenshotSpeakBtn->setStyleSheet(screenshotCopyBtn->styleSheet());
    connect(screenshotSpeakBtn, &QPushButton::clicked, this, [this]() {
        speakText(screenshotResult_ ? screenshotResult_->toPlainText() : QString());
    });
    resultBtnRow->addWidget(screenshotSpeakBtn);
    transFrameLayout->addLayout(resultBtnRow);
    transContainer->addWidget(transFrame, 1);
    resultInner->addLayout(transContainer, 1);
    contentRow->addWidget(resultCard, 1);
    lay->addLayout(contentRow, 1);

    // 参数行
    auto* ctrlRow = new QHBoxLayout;
    ctrlRow->addWidget(new QLabel(tr("Target Language:")));
    screenshotLangCombo_ = new QComboBox;
    populateLanguages(screenshotLangCombo_, QString(), true);
    ctrlRow->addWidget(screenshotLangCombo_);
    ctrlRow->addStretch();
    ctrlRow->addWidget(new QLabel(QStringLiteral("Max Tokens:")));
    screenshotMaxTokens_ = new QSpinBox;
    screenshotMaxTokens_->setRange(64, 4096);
    screenshotMaxTokens_->setValue(512);
    ctrlRow->addWidget(screenshotMaxTokens_);
    lay->addLayout(ctrlRow);

    return panel;
}

// —————— 图片翻译面板 ——————
QWidget* MainWindow::createPhotoPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // --- 拖拽区（复用 DropZoneWidget）---
    auto* dropZone = new DropZoneWidget;
    dropZone->setFixedHeight(90);
    auto* dropInner = new QVBoxLayout(dropZone);
    dropInner->setAlignment(Qt::AlignCenter);
    dropInner->setSpacing(2);
    auto* dropIcon = new QLabel;
    dropIcon->setPixmap(QPixmap(QStringLiteral(":/icons/photo.png")).scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    dropIcon->setAlignment(Qt::AlignCenter);
    dropIcon->setFixedSize(24, 24);
    dropInner->addWidget(dropIcon, 0, Qt::AlignCenter);
    auto* dropTxt = new QLabel(tr("Drag image here or click to upload"));
    dropTxt->setAlignment(Qt::AlignCenter);
    dropTxt->setStyleSheet("font-size: 13px; color: #6B7280; border: none;");
    dropInner->addWidget(dropTxt);
    auto* dropFmt = new QLabel(QStringLiteral("JPG / PNG / BMP / TIFF"));
    dropFmt->setAlignment(Qt::AlignCenter);
    dropFmt->setStyleSheet("font-size: 11px; color: #9CA3AF; border: none;");
    dropInner->addWidget(dropFmt);
    layout->addWidget(dropZone);

    connect(dropZone, &DropZoneWidget::fileDropped, this, [this](const QStringList& paths) {
        QString path;
        if (paths.isEmpty()) {
            path = QFileDialog::getOpenFileName(
                this, tr("Select Image"), QString(),
                tr("Images (*.png *.jpg *.jpeg *.bmp *.tiff);;All Files (*)"));
        } else {
            path = paths.first();
        }
        if (!path.isEmpty()) {
            photoInputPath_->setText(path);
            // 使用 cv::imread 加载原图
            cv::Mat img = cv::imread(path.toStdString());
            if (!img.empty()) {
                photoOrigMat_ = img.clone();
                photoZoom_ = 1.0;
                // cv::Mat BGR → QImage RGB
                cv::Mat rgb;
                cv::cvtColor(img, rgb, cv::COLOR_BGR2RGB);
                QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
                QPixmap pix = QPixmap::fromImage(qimg);
                if (!pix.isNull()) {
                    // 存全分辨率图用于缩放，显示适配缩略图
                    int maxW = photoInputScroll_->viewport()->width();
                    int maxH = photoInputScroll_->viewport()->height();
                    if (maxW < 50) maxW = 300;
                    if (maxH < 50) maxH = 200;
                    QPixmap displayPix = pix.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    auto* zoomLabel = qobject_cast<ZoomableLabel*>(photoPreview_);
                    if (zoomLabel) zoomLabel->setFullImage(pix, displayPix);
                    photoPreview_->setText(QString());
                    photoPreview_->setAlignment(Qt::AlignCenter);
                } else {
                    photoPreview_->setText(tr("Cannot load image"));
                }
            } else {
                photoPreview_->setText(tr("Cannot load image"));
            }
        }
    });

    // 隐藏字段
    photoInputPath_ = new QLineEdit;
    photoInputPath_->setVisible(false);
    layout->addWidget(photoInputPath_);

    // --- 左右预览对比 ---
    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(12);

    // 左: 原图
    auto* inputCard = new QFrame;
    inputCard->setObjectName("card");
    photoInputCard_ = inputCard;
    inputCard->installEventFilter(this);
    auto* inputInner = new QVBoxLayout(inputCard);
    inputInner->setContentsMargins(12, 8, 12, 8);
    inputInner->setSpacing(4);
    auto* inpHeader = new QHBoxLayout;
    inpHeader->setSpacing(4);
    auto* inpIcon = new QLabel;
    inpIcon->setFixedSize(16, 16);
    QPixmap inpPix(QStringLiteral(":/icons/org_image.png"));
    if (!inpPix.isNull())
        inpIcon->setPixmap(inpPix.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    inpHeader->addWidget(inpIcon);
    auto* inpLabel = new QLabel(tr("Original"));
    inpLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #889096; text-transform: uppercase;");
    inpHeader->addWidget(inpLabel);
    inpHeader->addStretch();

    // 清空原图按钮
    auto* photoInputClearBtn = new QPushButton(tr("Clear"));
    photoInputClearBtn->setCursor(Qt::PointingHandCursor);
    photoInputClearBtn->setStyleSheet(
        "QPushButton { border: none; background: transparent; color: #889096; font-size: 12px; padding: 2px 6px; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(11, 124, 114, 0.08); color: #0B7C72; }");
    connect(photoInputClearBtn, &QPushButton::clicked, this, [this]() {
        photoPreview_->clearImage();
        photoPreview_->setFixedSize(360, 240);
        photoPreview_->setText(tr("Image preview…"));
        photoInputPath_->clear();
        photoOrigMat_ = cv::Mat();
        auto* zl = qobject_cast<ZoomableLabel*>(photoOutputPreview_);
        if (zl) zl->clearImage();
        photoOutputPreview_->setText(tr("Result shows after translation…"));
        photoOutputMat_ = cv::Mat();
        QTimer::singleShot(0, this, [this]() {
            photoPreview_->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            photoPreview_->setMinimumSize(200, 140);
        });
    });
    inpHeader->addWidget(photoInputClearBtn);
    inputInner->addLayout(inpHeader);
    photoPreview_ = new ZoomableLabel;
    photoPreview_->setMinimumSize(200, 140);
    photoPreview_->setAlignment(Qt::AlignCenter);
    photoPreview_->setStyleSheet("background: #F5F7F7; border-radius:4px; color:#86868B; font-size: 13px;");
    photoPreview_->setText(tr("Image preview…"));
    photoInputScroll_ = new QScrollArea;
    photoInputScroll_->setWidget(photoPreview_);
    photoInputScroll_->setWidgetResizable(true);
    photoInputScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    photoInputScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    photoInputScroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    inputInner->addWidget(photoInputScroll_, 1);
    contentRow->addWidget(inputCard);

    // 右: 翻译后
    auto* outputCard = new QFrame;
    outputCard->setObjectName("card");
    photoOutputCard_ = outputCard;
    outputCard->installEventFilter(this);
    outputCard->setStyleSheet(
        "QFrame#card { background: #F0F7F6; border: 1px solid #D0E8E4; border-radius: 8px; }");
    auto* outputInner = new QVBoxLayout(outputCard);
    outputInner->setContentsMargins(12, 8, 12, 8);
    outputInner->setSpacing(4);
    auto* outHeader = new QHBoxLayout;
    outHeader->setSpacing(4);
    auto* outIcon = new QLabel;
    outIcon->setFixedSize(18, 18);
    outIcon->setStyleSheet("background: transparent; border: none; padding: 0; margin: 0;");
    QPixmap outPix(QStringLiteral(":/icons/trans.png"));
    if (!outPix.isNull())
        outIcon->setPixmap(outPix.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    outHeader->addWidget(outIcon);
    auto* outLabel2 = new QLabel(tr("Translation Result"));
    outLabel2->setStyleSheet("font-size: 12px; font-weight: 600; color: #0B7C72; text-transform: uppercase; background: transparent; border: none; padding: 0; margin: 0;");
    outHeader->addWidget(outLabel2);
    outHeader->addStretch();

    // 复制按钮
    photoCopyBtn_ = new QPushButton(tr("Copy"));
    photoCopyBtn_->setCursor(Qt::PointingHandCursor);
    photoCopyBtn_->setStyleSheet(
        "QPushButton { border: none; background: transparent; color: #0B7C72; font-size: 12px; padding: 2px 6px; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(11, 124, 114, 0.12); }");
    connect(photoCopyBtn_, &QPushButton::clicked, this, [this]() {
        auto* zl = qobject_cast<ZoomableLabel*>(photoOutputPreview_);
        if (zl && zl->hasImage()) {
            QApplication::clipboard()->setPixmap(zl->originalFullPixmap());
            statusBar_->showMessage(tr("Image copied"), 2000);
        }
    });
    outHeader->addWidget(photoCopyBtn_);

    // 下载按钮 — 弹出另存为选择保存目录
    photoDownloadBtn_ = new QPushButton(tr("Download"));
    photoDownloadBtn_->setCursor(Qt::PointingHandCursor);
    photoDownloadBtn_->setStyleSheet(photoCopyBtn_->styleSheet());
    connect(photoDownloadBtn_, &QPushButton::clicked, this, [this]() {
        if (photoOutputMat_.empty()) return;
        QString randName = QStringLiteral("AceTranslate_%1.png")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));
        QString savePath = QFileDialog::getSaveFileName(
            this, tr("Save Image"), randName,
            tr("PNG Images (*.png)"));
        if (savePath.isEmpty()) return;
        if (cv::imwrite(savePath.toStdString(), photoOutputMat_)) {
            statusBar_->showMessage(tr("Image saved: ") + QFileInfo(savePath).fileName(), 3000);
        } else {
            statusBar_->showMessage(tr("Save Image Failed"), 2000);
        }
    });
    outHeader->addWidget(photoDownloadBtn_);

    outputInner->addLayout(outHeader);
    photoOutputPreview_ = new ZoomableLabel;
    photoOutputPreview_->setAlignment(Qt::AlignCenter);
    photoOutputPreview_->setStyleSheet("background: #F5F7F7; border-radius:4px; color:#86868B; font-size: 13px;");
    photoOutputPreview_->setText(tr("Result shows after translation…"));
    photoOutputScroll_ = new QScrollArea;
    photoOutputScroll_->setWidget(photoOutputPreview_);
    photoOutputScroll_->setWidgetResizable(true);
    photoOutputScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    photoOutputScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    photoOutputScroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    outputInner->addWidget(photoOutputScroll_, 1);
    contentRow->addWidget(outputCard);
    layout->addLayout(contentRow, 1);  // 1 = stretch, 预览区填满剩余空间

    // --- 底部控制栏 ---
    auto* ctrlCard = new QFrame;
    ctrlCard->setObjectName("card");
    auto* ctrlInner = new QHBoxLayout(ctrlCard);
    ctrlInner->setContentsMargins(12, 8, 12, 8);
    ctrlInner->setSpacing(12);

    ctrlInner->addWidget(new QLabel(tr("Target Language:")));
    photoLangCombo_ = new QComboBox;
    populateLanguages(photoLangCombo_, QString(), true);
    ctrlInner->addWidget(photoLangCombo_);

    ctrlInner->addStretch();

    photoMaxTokens_ = new QSpinBox;
    photoMaxTokens_->setRange(64, 4096);
    photoMaxTokens_->setValue(512);
    photoMaxTokens_->setFixedWidth(80);
    ctrlInner->addWidget(new QLabel(QStringLiteral("Max Tokens:")));
    ctrlInner->addWidget(photoMaxTokens_);
    ctrlInner->addSpacing(12);
    auto* transBtn = new QPushButton(tr("Translate Image"));
    transBtn->setObjectName("primaryBtn");
    transBtn->setMinimumWidth(100);
    photoTranslateBtn_ = transBtn;
    connect(transBtn, &QPushButton::clicked, this, &MainWindow::onProcessPhoto);
    ctrlInner->addWidget(transBtn);

    layout->addWidget(ctrlCard);

    return panel;
}

// —————— 文件翻译面板 (带拖拽区) ——————
QWidget* MainWindow::createFilePanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // ========================================================
    // 1. 文件拖拽区 — 现代化设计
    // ========================================================
    fileDropZone_ = new DropZoneWidget;
    fileDropZone_->setFixedHeight(160);
    fileDropZone_->setStyleSheet(QStringLiteral(
        "DropZoneWidget { background: #F0F7F6; border: 2px dashed #D0E8E4; border-radius: 12px; }"
        "DropZoneWidget:hover { background: #E8F5F3; border-color: #0B7C72; }"
    ));
    auto* dropLayout = new QVBoxLayout(fileDropZone_);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(6);

    auto* dropIcon = new QLabel(QStringLiteral("\U0001F4C1"));
    dropIcon->setAlignment(Qt::AlignCenter);
    dropIcon->setStyleSheet("font-size: 32px; background: transparent;");
    dropLayout->addWidget(dropIcon);

    auto* dropText = new QLabel(tr("Drag files here or click to upload"));
    dropText->setAlignment(Qt::AlignCenter);
    dropText->setStyleSheet("font-size: 14px; font-weight: 600; color: #374151; background: transparent;");
    dropLayout->addWidget(dropText);

    auto* dropHint = new QLabel(tr("Supports PDF / Word / Excel / PPT / MD / TXT / Images"));
    dropHint->setAlignment(Qt::AlignCenter);
    dropHint->setStyleSheet("font-size: 12px; color: #9CA3AF; background: transparent;");
    dropLayout->addWidget(dropHint);

    auto* badgeRow = new QHBoxLayout;
    badgeRow->setAlignment(Qt::AlignCenter);
    badgeRow->setSpacing(5);
    for (const QString& fmt : {"PDF", "DOCX", "XLSX", "PPTX", "MD", "TXT", "JPG/PNG"}) {
        auto* badge = new QLabel(fmt);
        badge->setStyleSheet(
            "background: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 10px;"
            " padding: 2px 10px; font-size: 11px; font-weight: 500; color: #6B7280;");
        badgeRow->addWidget(badge);
    }
    dropLayout->addLayout(badgeRow);

    layout->addWidget(fileDropZone_);

    // 拖拽/点击事件
    connect(fileDropZone_, &DropZoneWidget::fileDropped, this, [this](const QStringList& paths) {
        auto addFile = [this](const QString& p) { addFileToList(p); filePendingPaths_.append(p); };
        if (paths.isEmpty()) {
            // 点击上传：支持多选
            QStringList files = QFileDialog::getOpenFileNames(
                this, tr("Select File"), QString(),
                QStringLiteral("支持的文件 (*.png *.jpg *.jpeg *.bmp *.tiff *.pdf *.docx *.xlsx *.pptx *.md *.txt);;"
                               "所有文件 (*)"));
            for (const QString& path : files) addFile(path);
        } else {
            for (const QString& p : paths) addFile(p);
        }
    });

    // ========================================================
    // 2. 文件列表 — 带标题 + 滚动条
    // ========================================================
    auto* fileListCard = new QFrame;
    fileListCard->setStyleSheet(QStringLiteral(
        "QFrame { background: #F5F7F7; border: 1px solid #E8ECEF; border-radius: 10px; }"
    ));
    auto* fileListCardLayout = new QVBoxLayout(fileListCard);
    fileListCardLayout->setContentsMargins(10, 8, 10, 8);
    fileListCardLayout->setSpacing(6);

    // 标题行
    auto* fileListHeader = new QHBoxLayout;
    auto* fileListTitle = new QLabel(tr("File List"));
    fileListTitle->setStyleSheet(
        "font-size: 12px; font-weight: 600; color: #889096; background: transparent;"
        " text-transform: uppercase;");
    fileListHeader->addWidget(fileListTitle);
    fileListHeader->addStretch();
    fileListCardLayout->addLayout(fileListHeader);

    // 滚动区域
    auto* fileListScroll = new QScrollArea;
    fileListScroll->setWidgetResizable(true);
    fileListScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    fileListScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fileListScroll->setMinimumHeight(100);
    fileListScroll->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 6px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #D1D1D6; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    fileListContainer_ = new QWidget;
    fileListContainer_->setStyleSheet("QWidget { background: transparent; }");
    fileListLayout_ = new QVBoxLayout(fileListContainer_);
    fileListLayout_->setContentsMargins(0, 0, 0, 0);
    fileListLayout_->setSpacing(4);
    fileListLayout_->addStretch();
    fileListScroll->setWidget(fileListContainer_);
    fileListCardLayout->addWidget(fileListScroll, 1);
    layout->addWidget(fileListCard, 1);

    // ========================================================
    // 3. 参数组框 — 简洁卡片样式
    // ========================================================
    auto* paramCard = new QFrame;
    paramCard->setObjectName("card");
    paramCard->setStyleSheet(QStringLiteral(
        "QFrame#card { background: #F0F7F6; border: 1px solid #D0E8E4; border-radius: 10px; }"
    ));
    auto* paramLayout = new QHBoxLayout(paramCard);
    paramLayout->setContentsMargins(14, 8, 14, 8);
    paramLayout->setSpacing(10);

    // 目标语言
    paramLayout->addWidget(new QLabel(tr("Target Language:")));
    fileLangCombo_ = new QComboBox;
    populateLanguages(fileLangCombo_, QString(), true);
    fileLangCombo_->setMaximumWidth(130);
    fileLangCombo_->setFixedHeight(28);
    fileLangCombo_->setStyleSheet(
        "QComboBox { border: 1px solid #D1D5DB; border-radius: 6px;"
        " padding: 2px 8px; font-size: 12px; background: white; }");
    paramLayout->addWidget(fileLangCombo_);

    // 分隔
    auto* sep1 = new QFrame;
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedWidth(1);
    sep1->setStyleSheet("color: #E5E7EB; background: #E5E7EB;");
    paramLayout->addWidget(sep1);

    // 版面阈值
    paramLayout->addWidget(new QLabel(tr("Layout Threshold:")));
    fileLayoutThreshold_ = new QDoubleSpinBox;
    fileLayoutThreshold_->setRange(0.0, 1.0);
    fileLayoutThreshold_->setSingleStep(0.05);
    fileLayoutThreshold_->setValue(0.5);
    fileLayoutThreshold_->setFixedWidth(70);
    fileLayoutThreshold_->setFixedHeight(28);
    fileLayoutThreshold_->setStyleSheet(
        "QDoubleSpinBox { border: 1px solid #D1D5DB; border-radius: 6px; padding: 2px 4px; font-size: 12px; }");
    paramLayout->addWidget(fileLayoutThreshold_);

    // PDF DPI
    paramLayout->addWidget(new QLabel(tr("DPI:")));
    filePdfDpi_ = new QSpinBox;
    filePdfDpi_->setRange(72, 600);
    filePdfDpi_->setValue(200);
    filePdfDpi_->setFixedWidth(65);
    filePdfDpi_->setFixedHeight(28);
    filePdfDpi_->setStyleSheet(
        "QSpinBox { border: 1px solid #D1D5DB; border-radius: 6px; padding: 2px 4px; font-size: 12px; }");
    paramLayout->addWidget(filePdfDpi_);

    // 分隔
    auto* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFixedWidth(1);
    sep2->setStyleSheet("color: #E5E7EB; background: #E5E7EB;");
    paramLayout->addWidget(sep2);

    // 开关
    fileEnableWarp_ = new QCheckBox(tr("Deskew"));
    fileEnableWarp_->setObjectName("toggleSwitch");
    fileEnableWarp_->setChecked(true);
    fileEnableWarp_->setStyleSheet(
        "QCheckBox { font-size: 12px; color: #374151; spacing: 4px; }");
    paramLayout->addWidget(fileEnableWarp_);

    fileEnableEnhance_ = new QCheckBox(tr("Enhance"));
    fileEnableEnhance_->setObjectName("toggleSwitch");
    fileEnableEnhance_->setChecked(false);
    fileEnableEnhance_->setStyleSheet(
        "QCheckBox { font-size: 12px; color: #374151; spacing: 4px; }");
    paramLayout->addWidget(fileEnableEnhance_);

    paramLayout->addStretch();

    // 翻译按钮
    fileTranslateBtn_ = new QPushButton(tr("Translate"));
    fileTranslateBtn_->setObjectName("primaryBtn");
    fileTranslateBtn_->setFixedHeight(32);
    fileTranslateBtn_->setMinimumWidth(90);
    fileProcessBtn_ = fileTranslateBtn_;
    connect(fileTranslateBtn_, &QPushButton::clicked, this, [this]() {
        if (!filePendingPaths_.isEmpty()) {
            fileCurrentIdx_ = 0;
            onProcessFile();
        }
    });
    paramLayout->addWidget(fileTranslateBtn_);

    // 隐藏字段（向后兼容）
    fileInputPath_ = new QLineEdit;
    fileInputPath_->setVisible(false);
    paramLayout->addWidget(fileInputPath_);

    layout->addWidget(paramCard);

    return panel;
}

void MainWindow::addFileToList(const QString& filePath) {
    QFileInfo fi(filePath);
    auto* fileItem = new QFrame;
    fileItem->setFrameShape(QFrame::NoFrame);
    fileItem->setFrameShadow(QFrame::Plain);
    fileItem->setLineWidth(0);
    fileItem->setStyleSheet(
        "QFrame { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 8px; }"
        "QFrame:hover { border-color: #0B7C72; }");
    auto* itemLayout = new QHBoxLayout(fileItem);
    itemLayout->setContentsMargins(10, 8, 10, 8);
    itemLayout->setSpacing(10);

    // 根据扩展名选图标
    QString ext = fi.suffix().toLower();
    QString iconRes;
    if (ext == "pdf") iconRes = ":/icons/PDF.png";
    else if (ext == "docx" || ext == "doc") iconRes = ":/icons/DOCX.png";
    else if (ext == "xlsx" || ext == "xls") iconRes = ":/icons/XLSX.png";
    else if (ext == "pptx" || ext == "ppt") iconRes = ":/icons/PPTX.png";
    else if (ext == "txt") iconRes = ":/icons/TXT.png";
    else if (ext == "md") iconRes = ":/icons/Markdown.png";
    else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tiff") iconRes = ":/icons/image.png";
    else iconRes = ":/icons/file.png";

    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(QPixmap(iconRes).scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setFixedSize(32, 32);
    iconLabel->setAlignment(Qt::AlignCenter);
    itemLayout->addWidget(iconLabel);

    // 文件名 + 大小/时间 — 单个 QLabel，无多余框
    qint64 bytes = fi.size();
    QString sizeStr;
    if (bytes < 1024) sizeStr = QStringLiteral("%1 B").arg(bytes);
    else if (bytes < 1024 * 1024) sizeStr = QStringLiteral("%1 KB").arg(bytes / 1024);
    else sizeStr = QStringLiteral("%1 MB").arg(bytes / (1024 * 1024));
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss");

    auto* nameLabel = new QLabel(
        QStringLiteral("<span style='color:#1C1C1E; font-size:13px; font-weight:500;'>%1</span><br>"
                       "<span style='color:#8E8E93; font-size:11px;'>%2 · %3</span>")
            .arg(fi.fileName(), sizeStr, timeStr));
    nameLabel->setStyleSheet("background: transparent; padding: 0; margin: 0; border: none;");
    itemLayout->addWidget(nameLabel, 1);

    // 存储文件路径用于双击打开
    fileItem->setProperty("filePath", filePath);
    fileItem->installEventFilter(this);

    auto* statusLabel = new QLabel(tr("Queued"));
    statusLabel->setStyleSheet("font-size: 11px; color: #0B7C72; font-weight: 500;");
    statusLabel->setProperty("fileStatus", true);
    itemLayout->addWidget(statusLabel);

    // 下载按钮（完成后显示）
    auto* downloadBtn = new QPushButton(tr("Download"));
    downloadBtn->setFixedHeight(24);
    downloadBtn->setCursor(Qt::PointingHandCursor);
    downloadBtn->setStyleSheet(
        "QPushButton { border: 1px solid #0B7C72; background: transparent; color: #0B7C72;"
        " font-size: 11px; font-weight: 500; border-radius: 5px; padding: 0 12px; }"
        "QPushButton:hover { background: #0B7C72; color: #FFFFFF; }");
    downloadBtn->setProperty("outputPath", QString());
    downloadBtn->hide();
    connect(downloadBtn, &QPushButton::clicked, this, [this, downloadBtn]() {
        QString outPath = downloadBtn->property("outputPath").toString();
        if (outPath.isEmpty()) return;
        QString randName = QStringLiteral("AceTranslate_%1%2")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"))
            .arg(QFileInfo(outPath).suffix().isEmpty() ? ".md" : "." + QFileInfo(outPath).suffix());
        QString savePath = QFileDialog::getSaveFileName(
            this, tr("Save File"), randName,
            tr("All Files (*)"));
        if (savePath.isEmpty()) return;
        if (QFile::exists(outPath)) {
            if (QFile::copy(outPath, savePath)) {
                // 同时复制 assets 目录（如果存在）
                {
                    QString srcAssets = QFileInfo(outPath).absolutePath() + "/assets";
                    QString dstAssets = QFileInfo(savePath).absolutePath() + "/assets";
                    if (QDir(srcAssets).exists()) {
                        copyDirectory(srcAssets, dstAssets);
                        QDir(srcAssets).removeRecursively();
                    }
                }
                // 删除源目录的 .md 和 assets
                QFile::remove(outPath);
                // 清除"翻译完成"状态
                if (statusLabel_) statusLabel_->setText(QString());
                if (statusIcon_) statusIcon_->hide();
                progressBar_->hide();
                QString dir = QFileInfo(savePath).absolutePath();
                statusBar_->showMessage(tr("Downloaded to: ") + savePath, 8000);
                // 可点击的"在文件夹中打开" — 按钮样式更明显
                auto* openFolderBtn = new QPushButton(tr("📂 Open in Folder"));
                openFolderBtn->setCursor(Qt::PointingHandCursor);
                openFolderBtn->setFixedHeight(26);
                openFolderBtn->setStyleSheet(
                    "QPushButton { background: #0B7C72; color: #FFFFFF; font-size: 12px; font-weight: 500;"
                    " border: none; border-radius: 5px; padding: 0 14px; }"
                    "QPushButton:hover { background: #09685F; }");
                connect(openFolderBtn, &QPushButton::clicked, this, [dir]() {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
                });
                statusBar_->addPermanentWidget(openFolderBtn);
                // 8秒后自动移除
                QTimer::singleShot(8000, openFolderBtn, &QPushButton::deleteLater);
            } else {
                QFile::remove(savePath);
                if (!QFile::copy(outPath, savePath))
                    statusBar_->showMessage(tr("Save Failed"), 2000);
            }
        }
    });
    itemLayout->addWidget(downloadBtn);

    auto* removeBtn = new QPushButton(QStringLiteral("✕"));
    removeBtn->setFixedSize(20, 20);
    removeBtn->setCursor(Qt::PointingHandCursor);
    removeBtn->setStyleSheet(
        "QPushButton { border: none; background: transparent; color: #9CA3AF; font-size: 11px; }"
        "QPushButton:hover { color: #EF4444; }");
    connect(removeBtn, &QPushButton::clicked, this, [this, fileItem, filePath]() {
        fileListLayout_->removeWidget(fileItem);
        fileItem->deleteLater();
        filePendingPaths_.removeAll(filePath);
    });
    itemLayout->addWidget(removeBtn);

    fileListLayout_->insertWidget(fileListLayout_->count() - 1, fileItem); // 在 stretch 前插入
}

// —————— 设置面板 ——————
// ============================================================
// 创建设置/配置页 — 加入滚动区域
// ============================================================
QWidget* MainWindow::createSettingsPanel() {
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("Settings"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(tr("Requires restart"));
    hint->setObjectName("sectionHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto& cfg = docmind::ConfigManager::getInstance();

    // ===== GroupBox 通用样式 =====
    auto groupStyle = QStringLiteral(
        "QGroupBox { background: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 8px;"
        " font-size: 13px; font-weight: 600; color: #134E4A;"
        " margin-top: 14px; padding: 18px 18px 12px 18px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left;"
        " padding: 2px 8px; margin-left: 8px;"
        " background: #E8F5F3; border: 1px solid #D0E8E4; border-radius: 6px;"
        " color: #0B7C72; font-size: 11px; }");

    // ===== 1. 默认语言 =====
    {
        auto* group = new QGroupBox(tr("Default Language"));
        group->setStyleSheet(groupStyle);
        auto* form = new QVBoxLayout(group);
        form->setSpacing(6);

        auto* langCombo = new QComboBox;
        langCombo->setEditable(true);
        langCombo->setInsertPolicy(QComboBox::NoInsert);
        langCombo->addItems({
            tr("Chinese"), tr("English"), tr("French"),
            tr("Portuguese"), tr("Spanish"), tr("Japanese"),
            tr("Turkish"), tr("Russian"), tr("Arabic"),
            tr("Korean"), tr("Thai"), tr("Italian"),
            tr("German"), tr("Vietnamese"), tr("Malay"),
            tr("Indonesian"), tr("Filipino"), tr("Hindi"),
            tr("Traditional Chinese"), tr("Polish"), tr("Czech"),
            tr("Dutch"), tr("Khmer"), tr("Burmese"),
            tr("Persian"), tr("Gujarati"), tr("Urdu"),
            tr("Telugu"), tr("Marathi"), tr("Hebrew"),
            tr("Bengali"), tr("Tamil"), tr("Ukrainian"),
            tr("Tibetan"), tr("Kazakh"), tr("Mongolian"),
            tr("Uyghur"), tr("Cantonese"),
        });
        langCombo->setCurrentText(QString::fromStdString(cfg.getDefaultLanguage()));
        connect(langCombo, &QComboBox::currentTextChanged, this, [&cfg](const QString& text) {
            cfg.setNestedString("defaults.target_language", text.toStdString());
            cfg.save();
        });
        form->addWidget(langCombo);
        layout->addWidget(group);
    }

    // ===== 1.5 界面语言 =====
    {
        auto* group = new QGroupBox(tr("Language"));
        group->setStyleSheet(groupStyle);
        auto* form = new QVBoxLayout(group);
        form->setSpacing(6);

        auto* uiLangCombo = new QComboBox;
        uiLangCombo->addItem(QStringLiteral("中文"), QStringLiteral("zh_CN"));
        uiLangCombo->addItem(QStringLiteral("English"), QStringLiteral("en_US"));
        uiLangCombo->addItem(QStringLiteral("日本語"), QStringLiteral("ja_JP"));

        // 读取当前设置
        std::string curLang = cfg.getNestedJson("defaults").value("ui_language", nlohmann::json("zh_CN")).get<std::string>();
        int idx = uiLangCombo->findData(QString::fromStdString(curLang));
        if (idx >= 0) uiLangCombo->setCurrentIndex(idx);

        connect(uiLangCombo, &QComboBox::currentIndexChanged, this, [&cfg, uiLangCombo]() {
            QString langCode = uiLangCombo->currentData().toString();
            cfg.setNestedString("defaults.ui_language", langCode.toStdString());
            cfg.save();
            MainWindow::switchLanguage(langCode);
            QMessageBox::information(nullptr, tr("Prompt"),
                tr("Language change takes effect after restart"));
        });
        form->addWidget(uiLangCombo);
        layout->addWidget(group);
    }

    // ===== 2. OCR 模型大小 =====
    {
        auto* group = new QGroupBox(tr("OCR Model Size"));
        group->setStyleSheet(groupStyle);
        auto* form = new QVBoxLayout(group);
        form->setSpacing(6);

        auto* ocrSizeCombo = new QComboBox;
        ocrSizeCombo->addItems({"Tiny", "Medium", "Small"});
        std::string curSize = cfg.getNestedJson("defaults").value("ocr_model_size", "tiny");
        if (curSize == "tiny") ocrSizeCombo->setCurrentIndex(0);
        else if (curSize == "medium") ocrSizeCombo->setCurrentIndex(1);
        else ocrSizeCombo->setCurrentIndex(2);

        connect(ocrSizeCombo, &QComboBox::currentIndexChanged, this, [&cfg](int idx) {
            std::string val = (idx == 0) ? "tiny" : (idx == 1) ? "medium" : "small";
            cfg.setNestedString("defaults.ocr_model_size", val);
            cfg.save();
        });
        form->addWidget(ocrSizeCombo);
        layout->addWidget(group);
    }

    // ===== 3. 翻译模型 =====
    {
        auto* group = new QGroupBox(tr("Translation Model"));
        group->setStyleSheet(groupStyle);
        auto* form = new QVBoxLayout(group);
        form->setSpacing(6);

        auto* transCombo = new QComboBox;
        // 扫描 models/translation/ 下的 .gguf 文件
        QString modelDir = QCoreApplication::applicationDirPath() + "/models/translation";
        QDir dir(modelDir);
        QStringList filters;
        filters << "*.gguf";
        QStringList ggufFiles = dir.entryList(filters, QDir::Files, QDir::Name);
        if (ggufFiles.isEmpty()) {
            // 回退硬编码列表
            ggufFiles = {
                QStringLiteral("Hy-MT2-1.8B-Q4_K_M.gguf"),
                QStringLiteral("Hy-MT2-1.8B-Q6_K.gguf"),
                QStringLiteral("HY-MT1.5-1.8B-Q4_K_M.gguf"),
                QStringLiteral("HY-MT1.5-1.8B-Q6_K.gguf"),
            };
        }
        transCombo->addItems(ggufFiles);
        std::string curTrans = cfg.getNestedJson("defaults").value("trans_model", "Hy-MT2-1.8B-Q4_K_M.gguf");
        int curIdx = ggufFiles.indexOf(QString::fromStdString(curTrans));
        if (curIdx < 0) curIdx = 0;
        transCombo->setCurrentIndex(curIdx);

        connect(transCombo, &QComboBox::currentIndexChanged, this, [&cfg, transCombo](int idx) {
            cfg.setNestedString("defaults.trans_model", transCombo->currentText().toStdString());
            cfg.save();
        });
        form->addWidget(transCombo);
        layout->addWidget(group);
    }

    // ===== 4. 引擎启动加载 =====
    {
        auto* group = new QGroupBox(tr("Load Engines on Startup"));
        group->setStyleSheet(groupStyle);
        auto* form = new QVBoxLayout(group);
        form->setSpacing(6);

        auto* loadHint = new QLabel(tr("Disable to reduce startup time. Engines load on demand."));
        loadHint->setStyleSheet("font-size: 11px; color: #889096; font-weight: normal;");
        loadHint->setWordWrap(true);
        form->addWidget(loadHint);

        auto makeLoadCheck = [&](const QString& label, const std::string& key) -> QCheckBox* {
            auto* check = new QCheckBox(label);
            check->setObjectName("toggleSwitch");
            check->setChecked(cfg.getNestedJson("defaults").value(key, true));
            connect(check, &QCheckBox::toggled, this, [&cfg, key](bool checked) {
                cfg.setNestedBool("defaults." + key, checked);
                cfg.save();
            });
            return check;
        };

        form->addWidget(makeLoadCheck(tr("OCR Engine"), "enable_ocr"));
        form->addWidget(makeLoadCheck(tr("Translator"), "enable_translator"));
        form->addWidget(makeLoadCheck(tr("Formula Recognition"), "enable_vlm"));
        form->addWidget(makeLoadCheck(tr("Layout Analysis"), "enable_layout"));
        form->addWidget(makeLoadCheck(tr("Image Correction"), "enable_docproc"));
        form->addWidget(makeLoadCheck(tr("Speech Recognition"), "enable_asr"));

        layout->addWidget(group);
    }

    // ===== 5. GPU 设置 =====
    {
        auto* gpuGroup = new QGroupBox(tr("GPU Settings"));
        gpuGroup->setStyleSheet(groupStyle);
        auto* gpuForm = new QVBoxLayout(gpuGroup);
        gpuForm->setSpacing(8);

        auto* gpuHint = new QLabel(tr("Disable GPU to reduce VRAM. Restart required."));
        gpuHint->setStyleSheet("font-size: 11px; color: #889096; font-weight: normal;");
        gpuHint->setWordWrap(true);
        gpuForm->addWidget(gpuHint);

        auto* gpuMasterCheck = new QCheckBox(tr("Enable All GPU Acceleration"));
        gpuMasterCheck->setObjectName("toggleSwitch");
        gpuMasterCheck->setChecked(true);
        gpuMasterCheck->setStyleSheet(
            "QCheckBox { font-size: 13px; font-weight: 600; color: #134E4A;"
            "  background: #F0F7F6; border-radius: 6px; padding: 8px 10px; }"
            "QCheckBox::indicator { width: 44px; height: 24px; border-radius: 12px;"
            "  border: none; background: #D0D4D8; }"
            "QCheckBox::indicator:checked { background: #0B7C72; }"
        );
        gpuForm->addWidget(gpuMasterCheck);

        auto* gpuSubWidget = new QWidget;
        auto* gpuSubLayout = new QVBoxLayout(gpuSubWidget);
        gpuSubLayout->setContentsMargins(28, 0, 0, 0);
        gpuSubLayout->setSpacing(4);

        auto* gpuSep = new QFrame;
        gpuSep->setFrameShape(QFrame::HLine);
        gpuSep->setFixedHeight(1);
        gpuSep->setStyleSheet("color: #E5E5EA; margin: 4px 0;");
        gpuSubLayout->addWidget(gpuSep);

        auto makeGpuCheck = [&](const QString& label, const std::string& configKey) -> QCheckBox* {
            auto* check = new QCheckBox(label);
            check->setObjectName("toggleSwitch");
            check->setChecked(cfg.getNestedJson("defaults").value(configKey, true));
            check->setEnabled(true);
            connect(check, &QCheckBox::toggled, this, [&cfg, configKey](bool checked) {
                cfg.setNestedBool("defaults." + configKey, checked);
                cfg.save();
            });
            return check;
        };

        auto* gpuLayoutCheck = makeGpuCheck(tr("Layout GPU"), "enable_gpu_layout");
        auto* gpuOcrCheck = makeGpuCheck(tr("OCR GPU"), "enable_gpu_ocr");
        auto* gpuVlmCheck = makeGpuCheck(tr("VLM GPU"), "enable_gpu_vlm");
        auto* gpuDocprocCheck = makeGpuCheck(tr("Image Correction GPU"), "enable_gpu_docproc");
        auto* gpuTransCheck = makeGpuCheck(tr("Translator GPU"), "enable_gpu_translator");
        auto* gpuAsrCheck = makeGpuCheck(tr("ASR GPU"), "enable_gpu_asr");

        gpuSubLayout->addWidget(gpuLayoutCheck);
        gpuSubLayout->addWidget(gpuOcrCheck);
        gpuSubLayout->addWidget(gpuVlmCheck);
        gpuSubLayout->addWidget(gpuDocprocCheck);
        gpuSubLayout->addWidget(gpuTransCheck);
        gpuSubLayout->addWidget(gpuAsrCheck);

        gpuForm->addWidget(gpuSubWidget);

        connect(gpuMasterCheck, &QCheckBox::toggled, this, [gpuLayoutCheck, gpuOcrCheck, gpuVlmCheck, gpuDocprocCheck, gpuTransCheck, gpuAsrCheck](bool checked) {
            gpuLayoutCheck->setChecked(checked);
            gpuOcrCheck->setChecked(checked);
            gpuVlmCheck->setChecked(checked);
            gpuDocprocCheck->setChecked(checked);
            gpuTransCheck->setChecked(checked);
            gpuAsrCheck->setChecked(checked);
        });

        layout->addWidget(gpuGroup);
    }

    layout->addStretch();
    scrollArea->setWidget(panel);
    return scrollArea;
}

// ============================================================
// 系统托盘
// ============================================================
void MainWindow::createTrayIcon() {
    // 确保不重复创建托盘图标
    if (trayIcon_) {
        trayIcon_->hide();
        delete trayIcon_;
        trayIcon_ = nullptr;
    }
    trayIcon_ = new QSystemTrayIcon(this);
    // 用圆角图标
    {
        QPixmap src(":/icons/LOGO.png");
        if (!src.isNull()) {
            int s = qMin(src.width(), src.height());
            QPixmap rounded(s, s);
            rounded.fill(Qt::transparent);
            QPainter p(&rounded);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addRoundedRect(0, 0, s, s, s * 0.18, s * 0.18);
            p.setClipPath(path);
            p.drawPixmap(QRect(0, 0, s, s), src);
            p.end();
            trayIcon_->setIcon(QIcon(rounded));
        } else {
            trayIcon_->setIcon(QIcon(":/icons/LOGO.png"));
        }
    }
    trayIcon_->setToolTip(QStringLiteral("AceTranslatePro"));

    trayMenu_ = new QMenu(this);
    trayMenu_->setObjectName(QStringLiteral("trayMenu"));
    trayMenu_->setStyleSheet(QStringLiteral(
        "QMenu#trayMenu {"
        "  background: #FFFFFF;"
        "  border: 1px solid #D1D1D6;"
        "  border-radius: 10px;"
        "  padding: 6px 0;"
        "}"
        "QMenu#trayMenu::item {"
        "  padding: 10px 22px;"
        "  font-size: 13px;"
        "  color: #1C1C1E;"
        "  border: none;"
        "}"
        "QMenu#trayMenu::item:selected {"
        "  background: #007AFF;"
        "  color: #FFFFFF;"
        "  border-radius: 6px;"
        "  margin: 2px 8px;"
        "  padding: 10px 14px;"
        "}"
        "QMenu#trayMenu::separator {"
        "  height: 1px;"
        "  background: #E5E5EA;"
        "  margin: 4px 12px;"
        "}"
    ));

    auto* showAction = trayMenu_->addAction(tr("Show Main Window"));
    connect(showAction, &QAction::triggered, this, &MainWindow::onShowWindow);

    trayMenu_->addSeparator();

    auto* quitAction = trayMenu_->addAction(tr("Exit"));
    connect(quitAction, &QAction::triggered, this, &MainWindow::onQuitApp);

    trayIcon_->setContextMenu(trayMenu_);
    connect(trayIcon_, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);

    trayIcon_->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        onShowWindow();
    }
}

void MainWindow::onShowWindow() {
    showNormal();
    activateWindow();
    raise();
}

void MainWindow::onQuitApp() {
    if (trayIcon_) {
        trayIcon_->hide();
        delete trayIcon_;
        trayIcon_ = nullptr;
    }
    QApplication::quit();
}

// ============================================================
// 全局热键注册
// ============================================================
void MainWindow::registerGlobalHotkeys() {
    // 先确保 ConfigManager 已加载
    docmind::ConfigManager::getInstance().load();

    // 从配置读取自定义快捷键
    {
        auto& cfg = docmind::ConfigManager::getInstance();
        std::string savedKey = cfg.getString("hotkey.float.key", "");
        std::string savedMods = cfg.getString("hotkey.float.mods", "");
        if (!savedKey.empty()) {
            floatHotkeyKey_ = std::stoi(savedKey);
            floatHotkeyMods_ = std::stoi(savedMods);
            if (floatHotkeyBtn_) {
                QString txt;
                if (floatHotkeyMods_ & MOD_CONTROL) txt += "Ctrl+";
                if (floatHotkeyMods_ & MOD_SHIFT) txt += "Shift+";
                if (floatHotkeyMods_ & MOD_ALT) txt += "Alt+";
                txt += QChar(floatHotkeyKey_);
                floatHotkeyBtn_->setText(txt);
            }
        }
    }

    // 截图热键
    BOOL ok = RegisterHotKey((HWND)winId(), screenshotHotkeyId_,
                              screenshotHotkeyMods_, screenshotHotkeyKey_);
    if (!ok) {
        qWarning("MainWindow: RegisterHotKey(screenshot) failed (error %lu)", GetLastError());
    }

    // 划词翻译热键: Ctrl+Shift+C
    ok = RegisterHotKey((HWND)winId(), floatHotkeyId_,
                         floatHotkeyMods_, floatHotkeyKey_);
    if (!ok) {
        qWarning("MainWindow: RegisterHotKey(float) failed (error %lu)", GetLastError());
    }
}

// ============================================================
// nativeEventFilter — 捕获全局热键消息 + 单实例显示请求
// ============================================================
bool MainWindow::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        if (msg->wParam == floatHotkeyId_) {
            onFloatHotkey();
            return true;
        }
        if (msg->wParam == screenshotHotkeyId_) {
            onScreenshotHotkey();
            return true;
        }
    }
    // 单实例通知：另一个实例请求本窗口显示到前台
    if (msg->message == WM_APP + 0x100) {
        onShowWindow();
        return true;
    }
    return false;
}

// ============================================================
// onFloatHotkey — 模拟 Ctrl+C 后读取剪贴板（修复修饰键冲突）
// ============================================================
void MainWindow::onFloatHotkey() {
    // 最小化主窗口以确保焦点不在本应用，让 keybd_event 能发送到其他窗口
    if (isVisible() && !isMinimized()) {
        showMinimized();
        Sleep(100);
    }

    // 释放可能仍按下的 Ctrl 和 Shift（热键消息消耗后 OS 仍视为按下状态）
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);

    // 等待按键释放完成
    Sleep(50);

    // 重新发送 Ctrl+C
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('C', 0, 0, 0);
    Sleep(50);
    keybd_event('C', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);

    // 等待剪贴板更新
    Sleep(300);

    // 读取剪贴板（带重试）
    QString text;
    for (int attempt = 0; attempt < 3 && text.isEmpty(); attempt++) {
        if (attempt > 0) Sleep(200);

        if (!OpenClipboard(nullptr)) {
            // 剪贴板忙，重试
            continue;
        }

        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hData));
            if (pData) {
                text = QString::fromWCharArray(pData);
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }

    if (!text.isEmpty()) {
        // 将悬浮窗显示到鼠标位置附近
        POINT pt;
        GetCursorPos(&pt);
        floatWindow_->move(pt.x + 20, pt.y - 40);
        floatWindow_->translateText(text);
    } else {
        // 回退：用 UI 中的语言按钮文本
        if (floatWindow_) {
            floatWindow_->show();
        }
    }
}

// ============================================================
// onScreenshotHotkey — 启动截图
// ============================================================
void MainWindow::onScreenshotHotkey() {
    hide();
    regionCapture_->startCapture();
}

// ============================================================
// 槽函数实现
// ============================================================

void MainWindow::onTranslateText() {
    if (busy_.loadRelaxed()) {
        statusBar_->showMessage(tr("Processing, please wait…"), 3000);
        return;
    }
    if (!textInput_) return;
    QString text = textInput_->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("Prompt"), tr("Enter text to translate"));
        return;
    }

    auto* worker = new TranslateWorker;
    worker->mode = TranslateWorker::TextTranslate;
    worker->inputText = text;
    // 使用目标语言下拉框
    QString lang = textLangCombo_ ? textLangCombo_->currentText() : tr("Chinese");
    worker->targetLang = lang;
    worker->maxTokens = 512;
    runWorker(worker);
}

void MainWindow::onCaptureComplete() {
    show();
    raise();
    activateWindow();

    // 切换到截图翻译面板
    onNavButtonClicked(2);

    cv::Mat mat = regionCapture_->capturedImage();
    if (mat.empty()) {
        statusBar_->showMessage(tr("Screenshot cancelled"), 3000);
        return;
    }

    // 确保 OCR 引擎已加载（如果启动时未勾选，现在懒加载）
    if (!docmind::GlobalEngineContext::getInstance().ensureOCREngine()) {
        QMessageBox::warning(this, tr("Screenshot Unavailable"),
                             tr("OCR engine failed to load. Please check if model files exist.\n"
                                "Path: models/ocr/[tiny/small/medium]/"));
        return;
    }

    // 显示预览（全分辨率 + 自动缩放适合视图的缩略图）
    cv::Mat rgbMat;
    cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
    QImage qimg(rgbMat.data, rgbMat.cols, rgbMat.rows, static_cast<int>(rgbMat.step), QImage::Format_RGB888);
    QPixmap fullPix = QPixmap::fromImage(qimg);
    screenshotFullPix_ = fullPix;
    // 根据 ScrollArea viewport 大小计算缩略图
    int maxW = screenshotScroll_ ? screenshotScroll_->viewport()->width() : 360;
    int maxH = screenshotScroll_ ? screenshotScroll_->viewport()->height() : 240;
    maxW = qMax(50, maxW);
    maxH = qMax(50, maxH);
    QPixmap thumbPix = fullPix.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    screenshotPreview_->setFullImage(fullPix, thumbPix);
    screenshotPreview_->setText(QString());

    // 启动翻译
    auto* worker = new TranslateWorker;
    worker->mode = TranslateWorker::ScreenshotTranslate;
    worker->screenshotMat = mat.clone();
    worker->targetLang = screenshotLangCombo_->currentText();
    worker->maxTokens = screenshotMaxTokens_->value();
    runWorker(worker);
}

void MainWindow::onSelectInputFile() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select File"),
        QString(),
        QStringLiteral("支持的文件 (*.png *.jpg *.jpeg *.bmp *.tiff *.pdf *.docx *.xlsx *.pptx *.md *.txt);;"
                       "图片 (*.png *.jpg *.jpeg *.bmp *.tiff);;"
                       "PDF (*.pdf);;"
                       "Office (*.docx *.xlsx *.pptx);;"
                       "Markdown (*.md);;"
                       "文本 (*.txt);;"
                       "所有文件 (*)")
    );
    if (!path.isEmpty()) {
        fileInputPath_->setText(path);
    }
}

void MainWindow::onProcessFile() {
    if (busy_.loadRelaxed()) {
        statusBar_->showMessage(tr("Processing, please wait…"), 3000);
        return;
    }
    if (filePendingPaths_.isEmpty() || fileCurrentIdx_ >= filePendingPaths_.size()) {
        fileCurrentIdx_ = 0;
        QMessageBox::warning(this, tr("Prompt"), tr("Drag or select input file"));
        return;
    }
    QString inputPath = filePendingPaths_[fileCurrentIdx_];
    fileInputPath_->setText(inputPath);

    auto* worker = new TranslateWorker;
    worker->mode = TranslateWorker::FileTranslate;
    worker->inputPath = inputPath;
    worker->outputPath = QString();
    worker->targetLang = fileLangCombo_->currentText();
    worker->layoutThreshold = static_cast<float>(fileLayoutThreshold_->value());
    worker->pdfDpi = filePdfDpi_->value();
    worker->enableWarp = fileEnableWarp_->isChecked();
    worker->enableEnhance = fileEnableEnhance_->isChecked();
    worker->currentFilePath = inputPath;
    // 更新文件列表中对应文件的状态为"翻译中"
    // 用 fileCurrentIdx_ 直接匹配 layout 中的位置
    int layoutIdx = 0;
    for (int i = 0; i < fileListLayout_->count(); ++i) {
        auto* item = fileListLayout_->itemAt(i);
        if (!item || !item->widget()) continue;
        if (layoutIdx == fileCurrentIdx_) {
            QList<QLabel*> labels = item->widget()->findChildren<QLabel*>();
            for (int li = 0; li < labels.size(); ++li) {
                if (labels[li]->property("fileStatus").isValid()) {
                    labels[li]->setText(tr("Translating…"));
                    labels[li]->setStyleSheet("font-size: 11px; color: #0B7C72; font-weight: 500;");
                    break;
                }
            }
            break;
        }
        ++layoutIdx;
    }
    runWorker(worker);
}

void MainWindow::onSelectPhotoInput() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select Image"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tiff);;All Files (*)")
    );
    if (!path.isEmpty()) {
        photoInputPath_->setText(path);
        QPixmap fullPix(path);
        if (!fullPix.isNull()) {
            int maxW = photoInputScroll_->viewport()->width();
            int maxH = photoInputScroll_->viewport()->height();
            if (maxW < 50) maxW = 300;
            if (maxH < 50) maxH = 200;
            QPixmap displayPix = fullPix.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            auto* zoomLabel = qobject_cast<ZoomableLabel*>(photoPreview_);
            if (zoomLabel) zoomLabel->setFullImage(fullPix, displayPix);
            photoPreview_->setText(QString());
            photoPreview_->setAlignment(Qt::AlignCenter);
        } else {
            photoPreview_->setText(tr("Cannot load preview"));
        }
    }
}

void MainWindow::onProcessPhoto() {
    if (busy_.loadRelaxed()) {
        statusBar_->showMessage(tr("Processing, please wait…"), 3000);
        return;
    }
    if (!photoInputPath_) return;
    QString inputPath = photoInputPath_->text().trimmed();
    if (inputPath.isEmpty()) {
        QMessageBox::warning(this, tr("Prompt"), tr("Select input image"));
        return;
    }

    auto* worker = new TranslateWorker;
    worker->mode = TranslateWorker::PhotoTranslate;
    worker->inputPath = inputPath;
    worker->outputPath = QString();
    worker->baseDir = "";
    worker->targetLang = photoLangCombo_->currentText();
    worker->maxTokens = photoMaxTokens_ ? photoMaxTokens_->value() : 512;
    runWorker(worker);
}

void MainWindow::onBrowseBaseDir() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Model/DLL Directory"));
    if (!dir.isEmpty()) {
        baseDirPath_->setText(dir);
    }
}

// ============================================================
// 工作线程管理
// ============================================================

void MainWindow::runWorker(TranslateWorker* worker) {
    busy_.storeRelaxed(1);

    // 禁用所有操作按钮
    if (textTranslateBtn_) textTranslateBtn_->setEnabled(false);
    if (fileProcessBtn_) fileProcessBtn_->setEnabled(false);
    if (screenshotCaptureBtn_) screenshotCaptureBtn_->setEnabled(false);
    if (photoTranslateBtn_) photoTranslateBtn_->setEnabled(false);

    QThread* thread = new QThread(this);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &TranslateWorker::run);
    connect(worker, &TranslateWorker::finished, this, &MainWindow::onWorkerFinished);
    connect(worker, &TranslateWorker::ocrFinished, this, [this](const QString& ocrText) {
        if (screenshotOcrResult_) {
            screenshotOcrResult_->setPlainText(ocrText);
        }
    });
    connect(worker, &TranslateWorker::errorOccurred, this, &MainWindow::onWorkerError);
    connect(worker, &TranslateWorker::progressUpdated, this, &MainWindow::onWorkerProgress);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    progressBar_->show();
    if (statusLabel_) statusLabel_->setText(tr("Processing…"));
    if (hourglassTimer_) {
        hourglassTimer_->start();
        // 让沙漏立即显示
        onHourglassTick();
    }
    thread->start();
}

// 安全加载 ASR DLL（用 SEH 保护避免因缺少依赖闪退）
static HMODULE tryLoadASRDLL() {
    __try {
        return LoadLibraryA("asr.dll");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// ============================================================
// onMicButtonClicked — 语音输入
// ============================================================
void MainWindow::onMicButtonClicked() {
    // Shift+点击：从 test.wav 读取音频进行测试（无需麦克风）
    bool testMode = (QApplication::keyboardModifiers() & Qt::ShiftModifier);

    if (!isRecording_) {
        // 开始录音
        isRecording_ = true;
        textMicBtn_->setStyleSheet(
            "QPushButton { border: 0px; background: #E74C3C; color: white; font-size: 14px; padding: 2px 6px; border-radius: 4px; }"
            "QPushButton:focus { outline: none; }");
        textMicBtn_->setToolTip(tr("Click to stop recording"));
        statusBar_->showMessage(testMode ? tr("Test mode: reading test.wav…") : tr("Recording… click to stop"), 0);
        QString baseDir = QCoreApplication::applicationDirPath();

        // 在后台线程录音（避免阻塞 UI）
        asrThread_ = QThread::create([this, baseDir, testMode]() {
            // 加载 ASR DLL（用 SEH 保护，避免因缺少 CUDA 依赖闪退）
            typedef void* (*ASRCreateFunc)(const char*, const char*, const char*, int);
            typedef char* (*ASRRecognizeFunc)(void*, const short*, int);
            typedef void (*ASRDestroyFunc)(void*);

            HMODULE asrDll = tryLoadASRDLL();
            if (!asrDll) {
                QMetaObject::invokeMethod(this, [this]() {
                    statusBar_->showMessage(tr("ASR Engine Not Loaded"), 3000);
                    resetMicButton();
                }, Qt::QueuedConnection);
                return;
            }

            auto asrCreate = (ASRCreateFunc)GetProcAddress(asrDll, "asr_create");
            auto asrRecognize = (ASRRecognizeFunc)GetProcAddress(asrDll, "asr_recognize");
            auto asrDestroy = (ASRDestroyFunc)GetProcAddress(asrDll, "asr_destroy");
            if (!asrCreate || !asrRecognize || !asrDestroy) {
                FreeLibrary(asrDll);
                QMetaObject::invokeMethod(this, [this]() {
                    statusBar_->showMessage(tr("ASR DLL Interface Error"), 3000);
                    resetMicButton();
                }, Qt::QueuedConnection);
                return;
            }

            // 初始化 ASR 引擎（模型路径）
            std::string modelPath = baseDir.toStdString() + "\\models\\ASR\\model_quant.onnx";
            std::string tokensPath = baseDir.toStdString() + "\\models\\ASR\\tokens.json";
            std::string mvnPath = baseDir.toStdString() + "\\models\\ASR\\am.mvn";
            void* asrHandle = asrCreate(modelPath.c_str(), tokensPath.c_str(), mvnPath.c_str(), 0);
            if (!asrHandle) {
                FreeLibrary(asrDll);
                QMetaObject::invokeMethod(this, [this]() {
                    statusBar_->showMessage(tr("ASR Model Load Failed"), 3000);
                    resetMicButton();
                }, Qt::QueuedConnection);
                return;
            }

            // 开始录音（或从文件读取）
            std::vector<short> allData;
            if (testMode) {
                // 测试模式：从 exe 同目录的 test.wav 读取 16kHz 16-bit mono PCM
                std::string wavPath = baseDir.toStdString() + "\\test.wav";
                FILE* f = fopen(wavPath.c_str(), "rb");
                if (!f) {
                    asrDestroy(asrHandle);
                    FreeLibrary(asrDll);
                    QMetaObject::invokeMethod(this, [this]() {
                        statusBar_->showMessage(tr("Test mode: test.wav not found"), 3000);
                        resetMicButton();
                    }, Qt::QueuedConnection);
                    return;
                }
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                // 搜索 data chunk（WAV 头部可能大于 44 字节）
                long dataStart = 0;
                long dataBytes = 0;
                char chunkId[5] = {};
                unsigned int chunkSize = 0;
                fread(chunkId, 1, 4, f); // "RIFF"
                fread(&chunkSize, 4, 1, f);
                fread(chunkId, 1, 4, f); // "WAVE"
                while (fread(chunkId, 1, 4, f) == 4) {
                    chunkId[4] = '\0';
                    fread(&chunkSize, 4, 1, f);
                    if (strcmp(chunkId, "data") == 0) {
                        dataStart = ftell(f);
                        dataBytes = chunkSize;
                        break;
                    }
                    fseek(f, chunkSize, SEEK_CUR);
                }
                if (dataBytes > 0) {
                    allData.resize(dataBytes / 2);
                    fread(allData.data(), 1, dataBytes, f);
                }
                fclose(f);
                QMetaObject::invokeMethod(this, [this]() {
                    statusBar_->showMessage(tr("Test mode: loaded, recognizing"), 2000);
                }, Qt::QueuedConnection);
                // 跳到识别
                goto recognize;
            }

            {
                HWAVEIN hWaveIn = nullptr;
                WAVEHDR waveHeaders[2] = {};
                const int bufSamples = 16000 * 5;
                std::vector<short> bufs[2];
                allData.reserve(bufSamples * 4);

                WAVEFORMATEX wfx = {};
                wfx.wFormatTag = WAVE_FORMAT_PCM;
                wfx.nChannels = 1;
                wfx.nSamplesPerSec = 16000;
                wfx.wBitsPerSample = 16;
                wfx.nBlockAlign = 2;
                wfx.nAvgBytesPerSec = 32000;

                MMRESULT res = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx,
                                           (DWORD_PTR)WaveInProc, (DWORD_PTR)&allData,
                                           CALLBACK_FUNCTION);
                if (res != MMSYSERR_NOERROR) {
                    asrDestroy(asrHandle);
                    FreeLibrary(asrDll);
                    QMetaObject::invokeMethod(this, [this]() {
                        statusBar_->showMessage(tr("Microphone open failed"), 3000);
                        resetMicButton();
                    }, Qt::QueuedConnection);
                    return;
                }

                for (int i = 0; i < 2; ++i) {
                    bufs[i].resize(bufSamples, 0);
                    waveHeaders[i].lpData = (LPSTR)bufs[i].data();
                    waveHeaders[i].dwBufferLength = bufSamples * sizeof(short);
                    waveHeaders[i].dwFlags = 0;
                    waveInPrepareHeader(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
                    waveInAddBuffer(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
                }
                waveInStart(hWaveIn);

                // 等待用户停止录音
                while (isRecording_) {
                    Sleep(100);
                }

                // 停止录音
                waveInStop(hWaveIn);
                waveInReset(hWaveIn);
                for (int i = 0; i < 2; ++i) {
                    waveInUnprepareHeader(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
                }
                waveInClose(hWaveIn);
            }

recognize:
            // 识别
            if (!allData.empty()) {
                char* result = asrRecognize(asrHandle, allData.data(), (int)allData.size());
                if (result) {
                    QString text = QString::fromUtf8(result);
                    delete[] result;
                    QMetaObject::invokeMethod(this, [this, text]() {
                        if (textInput_) {
                            textInput_->insertPlainText(text);
                            textInput_->moveCursor(QTextCursor::End);
                        }
                        statusBar_->showMessage(tr("Speech complete"), 2000);
                    }, Qt::QueuedConnection);
                }
            }

            asrDestroy(asrHandle);
            FreeLibrary(asrDll);

            QMetaObject::invokeMethod(this, [this]() {
                resetMicButton();
            }, Qt::QueuedConnection);
        });
        asrThread_->start();
    } else {
        // 停止录音
        isRecording_ = false;
        statusBar_->showMessage(tr("Speech recognition…"));
    }
}

void MainWindow::resetMicButton() {
    isRecording_ = false;
    if (textMicBtn_) {
        textMicBtn_->setStyleSheet(
            "QPushButton { border: 0px; background: transparent; padding: 2px 4px; border-radius: 4px; }"
            "QPushButton:hover { background: rgba(11, 124, 114, 0.12); }"
            "QPushButton:focus { outline: none; }");
        textMicBtn_->setToolTip(tr("Voice Input"));
        textMicBtn_->setEnabled(true);
    }
}

// 静态回调：waveIn 数据回调
static void CALLBACK WaveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                 DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (uMsg != WIM_DATA) return;
    auto* allData = reinterpret_cast<std::vector<short>*>(dwInstance);
    auto* header = reinterpret_cast<WAVEHDR*>(dwParam1);
    if (!allData || !header) return;

    int samples = header->dwBytesRecorded / sizeof(short);
    if (samples > 0) {
        auto* data = reinterpret_cast<short*>(header->lpData);
        size_t old = allData->size();
        allData->resize(old + samples);
        memcpy(allData->data() + old, data, samples * sizeof(short));
    }

    // 重新提交
    header->dwBytesRecorded = 0;
    header->dwFlags = 0;
    waveInPrepareHeader(hwi, header, sizeof(WAVEHDR));
    waveInAddBuffer(hwi, header, sizeof(WAVEHDR));
}

void MainWindow::onWorkerFinished(const QString& result) {
    if (hourglassTimer_) hourglassTimer_->stop();
    if (statusLabel_) statusLabel_->setText(QString()); // 清旧消息
    if (statusIcon_) {
        statusIcon_->setText(QString()); // 清除沙漏文字
        statusIcon_->hide();
    }
    progressBar_->hide();
    QApplication::processEvents(); // 立即刷新进度条隐藏
    busy_.storeRelaxed(0);

    switch (currentNavIndex_) {
    case 0: // Text
        if (textOutput_) textOutput_->setPlainText(result);
        if (statusIcon_) {
            QPixmap yesIcon(":/icons/yes.png");
            statusIcon_->setPixmap(yesIcon.isNull() ? createCheckIcon() : yesIcon.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            statusIcon_->show();
        }
        if (statusLabel_) statusLabel_->setText(tr(" Translation Complete"));
        break;
    case 2: // Screenshot
        if (screenshotResult_) screenshotResult_->setPlainText(result);
        if (statusIcon_) {
            QPixmap yesIcon(":/icons/yes.png");
            statusIcon_->setPixmap(yesIcon.isNull() ? createCheckIcon() : yesIcon.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            statusIcon_->show();
        }
        if (statusLabel_) statusLabel_->setText(tr(" Translation Complete"));
        break;
    case 3: // Photo — 输出预览缩略图
        if (statusLabel_) statusLabel_->setText(tr(" Translation Complete"));
        if (statusIcon_) {
            QPixmap yesIcon(":/icons/yes.png");
            statusIcon_->setPixmap(yesIcon.isNull() ? createCheckIcon() : yesIcon.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            statusIcon_->show();
        }
        if (photoOutputPreview_) {
            QString outputPath = result;
            photoLastSavedPath_ = outputPath;
            cv::Mat outImg = cv::imread(outputPath.toStdString());
            if (!outImg.empty()) {
                photoOutputMat_ = outImg.clone();
                cv::Mat rgb;
                cv::cvtColor(outImg, rgb, cv::COLOR_BGR2RGB);
                QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
                QPixmap pix = QPixmap::fromImage(qimg);
                if (!pix.isNull()) {
                    int maxW = photoOutputScroll_ ? photoOutputScroll_->viewport()->width() : 300;
                    int maxH = photoOutputScroll_ ? photoOutputScroll_->viewport()->height() : 200;
                    if (maxW < 50) maxW = 300;
                    if (maxH < 50) maxH = 200;
                    QPixmap displayPix = pix.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    auto* zoomLabel = qobject_cast<ZoomableLabel*>(photoOutputPreview_);
                    if (zoomLabel) zoomLabel->setFullImage(pix, displayPix);
                    photoOutputPreview_->setText(QString());
                    photoOutputPreview_->setAlignment(Qt::AlignCenter);
                } else {
                    photoOutputPreview_->setText(outputPath);
                }
            } else {
                photoOutputPreview_->setText(outputPath);
            }
            // 删除临时文件
            QFile::remove(outputPath);
        }
        break;
    case 4: // File — 状态栏显示结果 + 更新文件状态 + 显示下载按钮
        // 更新当前文件的状态为已完成
        if (fileCurrentIdx_ < filePendingPaths_.size()) {
            int layoutIdx = 0;
            for (int i = 0; i < fileListLayout_->count(); ++i) {
                auto* item = fileListLayout_->itemAt(i);
                if (!item || !item->widget()) continue;
                if (layoutIdx == fileCurrentIdx_) {
                    QList<QLabel*> labels = item->widget()->findChildren<QLabel*>();
                    for (int li = 0; li < labels.size(); ++li) {
                        if (labels[li]->property("fileStatus").isValid()) {
                            labels[li]->setText(tr("Completed"));
                            labels[li]->setStyleSheet("font-size: 11px; color: #10B981;");
                            break;
                        }
                    }
                    QList<QPushButton*> btns = item->widget()->findChildren<QPushButton*>();
                    for (int bi = 0; bi < btns.size(); ++bi) {
                        if (btns[bi]->text() == tr("Download")) {
                            btns[bi]->setProperty("outputPath", result);
                            btns[bi]->show();
                            break;
                        }
                    }
                    break;
                }
                ++layoutIdx;
            }
        }
        if (statusIcon_) {
            QPixmap yesIcon(":/icons/yes.png");
            statusIcon_->setPixmap(yesIcon.isNull() ? createCheckIcon() : yesIcon.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            statusIcon_->show();
        }
        statusLabel_->setText(tr(" Translation Complete: %1").arg(result));
        // 自动翻译下一个文件
        ++fileCurrentIdx_;
        if (fileCurrentIdx_ < filePendingPaths_.size()) {
            QTimer::singleShot(500, this, &MainWindow::onProcessFile);
        } else {
            fileCurrentIdx_ = 0;
            // 全部翻译完成 → 悬浮通知（带图标）
            int total = filePendingPaths_.size();
            QString msg = tr("All %1 files translated").arg(total);
            ToastNotification::show(this, msg, 4000, QColor(11, 124, 114));
        }
        break;
    }
    busy_.storeRelaxed(0);
    if (textTranslateBtn_) textTranslateBtn_->setEnabled(true);
    if (fileProcessBtn_) fileProcessBtn_->setEnabled(true);
    if (screenshotCaptureBtn_) screenshotCaptureBtn_->setEnabled(true);
    if (photoTranslateBtn_) photoTranslateBtn_->setEnabled(true);

    // 5秒后清除翻译完成状态（如果没被新操作覆盖）
    QTimer::singleShot(5000, this, [this]() {
        if (statusLabel_ && statusLabel_->text().contains(tr("Translation Complete"))) {
            statusLabel_->setText(tr("Ready"));
            if (statusIcon_) statusIcon_->hide();
        }
    });
}

void MainWindow::onWorkerError(const QString& err) {
    if (hourglassTimer_) hourglassTimer_->stop();
    progressBar_->hide();
    statusBar_->showMessage(tr("Error"), 5000);
    QMessageBox::warning(this, tr("Translation Error"), err);

    busy_.storeRelaxed(0);
    if (textTranslateBtn_) textTranslateBtn_->setEnabled(true);
    if (fileProcessBtn_) fileProcessBtn_->setEnabled(true);
    if (screenshotCaptureBtn_) screenshotCaptureBtn_->setEnabled(true);
    if (photoTranslateBtn_) photoTranslateBtn_->setEnabled(true);
}

void MainWindow::onWorkerProgress(const QString& msg) {
    if (statusLabel_) statusLabel_->setText(msg);
}

// ============================================================
// onHourglassTick — ⏳ 旋转绘制
// ============================================================
void MainWindow::onHourglassTick() {
    if (!statusIcon_) return;
    QPixmap pix(18, 18);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.translate(9, 9);
        p.rotate(hourglassFrame_ * 30.0); // 每帧转30度
        QFont f = p.font();
        f.setPixelSize(16);
        p.setFont(f);
        p.setPen(QColor(11, 124, 114)); // #0B7C72
        p.drawText(QRect(-9, -9, 18, 18), Qt::AlignCenter, QStringLiteral("\xe2\x8f\xb3")); // ⏳
    }
    statusIcon_->setPixmap(pix);
    statusIcon_->setFixedSize(16, 16);
    statusIcon_->show();
    hourglassFrame_++;
}

// ============================================================
// speakText — 使用 Windows SAPI 朗读文本（自动匹配语音）
// ============================================================
void MainWindow::speakText(const QString& text) {
    if (text.isEmpty()) return;

    // 检测文本包含哪些非拉丁文字
    bool hasChinese = false, hasKorean = false, hasJapanese = false;
    bool hasTamil = false, hasHindi = false, hasBengali = false;
    bool hasTelugu = false, hasMarathi = false, hasGujarati = false;
    bool hasMalayalam = false, hasKannada = false, hasPunjabi = false;
    bool hasArabic = false, hasHebrew = false, hasThai = false;
    bool hasKhmer = false, hasBurmese = false, hasTibetan = false;
    bool hasMongolian = false, hasRussian = false;

    for (const QChar& c : text) {
        uint32_t u = c.unicode();
        if      (u >= 0x4E00 && u <= 0x9FFF) hasChinese = true;
        else if (u >= 0xAC00 && u <= 0xD7AF) hasKorean = true;
        else if (u >= 0x3040 && u <= 0x30FF) hasJapanese = true;
        else if (u >= 0x0900 && u <= 0x097F) { hasHindi = true; hasMarathi = true; }
        else if (u >= 0x0980 && u <= 0x09FF) hasBengali = true;
        else if (u >= 0x0A00 && u <= 0x0A7F) hasPunjabi = true;
        else if (u >= 0x0A80 && u <= 0x0AFF) hasGujarati = true;
        else if (u >= 0x0B80 && u <= 0x0BFF) hasTamil = true;
        else if (u >= 0x0C00 && u <= 0x0C7F) hasTelugu = true;
        else if (u >= 0x0C80 && u <= 0x0CFF) hasKannada = true;
        else if (u >= 0x0D00 && u <= 0x0D7F) hasMalayalam = true;
        else if (u >= 0x0E00 && u <= 0x0E7F) hasThai = true;
        else if (u >= 0x1780 && u <= 0x17FF) hasKhmer = true;
        else if (u >= 0x1000 && u <= 0x109F) hasBurmese = true;
        else if (u >= 0x0600 && u <= 0x06FF) hasArabic = true;
        else if (u >= 0x0590 && u <= 0x05FF) hasHebrew = true;
        else if (u >= 0x0F00 && u <= 0x0FFF) hasTibetan = true;
        else if (u >= 0x1800 && u <= 0x18AF) hasMongolian = true;
        else if (u >= 0x0400 && u <= 0x04FF) hasRussian = true;
    }

    ISpVoice* pVoice = nullptr;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice);
    if (FAILED(hr) || !pVoice) {
        qWarning() << "SAPI voice initialization failed:" << hr;
        return;
    }

    // 枚举所有语音
    IEnumSpObjectTokens* pEnum = nullptr;
    hr = SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &pEnum);
    if (SUCCEEDED(hr) && pEnum) {
        struct VoiceInfo { ISpObjectToken* token; std::wstring name; std::wstring lcid; };
        std::vector<VoiceInfo> voices;
        ISpObjectToken* pToken = nullptr;
        while (pEnum->Next(1, &pToken, nullptr) == S_OK) {
            LPWSTR lang = nullptr, vname = nullptr;
            pToken->GetStringValue(L"Language", &lang);
            pToken->GetStringValue(L"", &vname);
            voices.push_back({pToken, vname ? vname : L"", lang ? lang : L""});
            CoTaskMemFree(lang); CoTaskMemFree(vname);
        }
        pEnum->Release();

        if (!voices.empty()) {
            // 确定目标语言的 LCID 和名称关键词
            struct LangInfo { std::wstring lcid; std::vector<std::wstring> names; };
            LangInfo target;

            if (hasKorean)        target = {L"0412", {L"Korean", L"한"}};
            else if (hasJapanese) target = {L"0411", {L"Japanese", L"日"}};
            else if (hasChinese)  target = {L"0804", {L"Chinese", L"中文", L"Hui", L"Xiao", L"Yun"}};
            else if (hasTamil)    target = {L"0449", {L"Tamil"}};
            else if (hasHindi || hasMarathi) target = {L"0439", {L"Hindi", L"द"}};
            else if (hasBengali)  target = {L"0445", {L"Bengali", L"ব"}};
            else if (hasTelugu)   target = {L"044A", {L"Telugu", L"త"}};
            else if (hasGujarati) target = {L"0447", {L"Gujarati", L"ગ"}};
            else if (hasMalayalam) target = {L"044C", {L"Malayalam", L"മ"}};
            else if (hasKannada)  target = {L"044B", {L"Kannada", L"ಕ"}};
            else if (hasPunjabi)  target = {L"0446", {L"Punjabi", L"ਪ"}};
            else if (hasThai)     target = {L"041E", {L"Thai"}};
            else if (hasArabic)   target = {L"0401", {L"Arabic"}};
            else if (hasHebrew)   target = {L"040D", {L"Hebrew"}};
            else if (hasKhmer)    target = {L"0453", {L"Khmer"}};
            else if (hasBurmese)  target = {L"0455", {L"Burmese"}};
            else if (hasTibetan)  target = {L"0451", {L"Tibetan"}};
            else if (hasMongolian) target = {L"0450", {L"Mongolian"}};
            else if (hasRussian)  target = {L"0419", {L"Russian", L"Russian"}};
            else                  target = {L"0409", {L"English", L"David", L"Zira", L"Jenny"}};

            bool matched = false;
            for (auto& v : voices) {
                if (!target.lcid.empty() && v.lcid.find(target.lcid) != std::wstring::npos) {
                    pVoice->SetVoice(v.token);
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                for (auto& v : voices) {
                    for (const auto& kw : target.names) {
                        if (v.name.find(kw) != std::wstring::npos) {
                            pVoice->SetVoice(v.token);
                            matched = true;
                            break;
                        }
                    }
                    if (matched) break;
                }
            }
            for (auto& v : voices) v.token->Release();

            // 没匹配到 — 弹提示
            if (!matched) {
                QStringList names;
                if (hasKorean)     names << tr("Korean");
                else if (hasJapanese) names << tr("Japanese");
                else if (hasTamil)  names << tr("Tamil");
                else if (hasHindi)  names << tr("Hindi");
                else if (hasBengali) names << tr("Bengali");
                else if (hasTelugu) names << tr("Telugu");
                else if (hasMarathi) names << tr("Marathi");
                else if (hasGujarati) names << tr("Gujarati");
                else if (hasMalayalam) names << tr("Malayalam");
                else if (hasKannada) names << tr("Kannada");
                else if (hasPunjabi) names << tr("Punjabi");
                else if (hasThai)   names << tr("Thai");
                else if (hasArabic) names << tr("Arabic");
                else if (hasHebrew) names << tr("Hebrew");
                else if (hasKhmer)  names << tr("Khmer");
                else if (hasBurmese) names << tr("Burmese");
                else if (hasTibetan) names << tr("Tibetan");
                else if (hasMongolian) names << tr("Mongolian");
                else if (hasRussian) names << tr("Russian");

                if (!names.isEmpty()) {
                    ToastNotification::show(this,
                        tr("Voice %1 not found").arg(names.join(QStringLiteral("/"))), 5000,
                        QColor("#E74C3C"), QStringLiteral(":/icons/close.png"),
                        tr("Open Settings"),
                        QStringLiteral("ms-settings:speech"));
                }
            }
        }
    }

    std::wstring wstr = text.toStdWString();
    pVoice->Speak(wstr.c_str(), SPF_DEFAULT, nullptr);
    pVoice->Release();
}

// ============================================================
// createCheckIcon — 绘制绿色小对勾图标
// ============================================================
QPixmap MainWindow::createCheckIcon() {
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    // 绿色圆底
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#10B981"));
    painter.drawEllipse(1, 1, 14, 14);
    // 白色对勾
    painter.setPen(QPen(Qt::white, 2.2));
    painter.drawLine(QPointF(4.5, 8.5), QPointF(7, 11));
    painter.drawLine(QPointF(7, 11), QPointF(12, 5.5));
    painter.end();
    return pix;
}
