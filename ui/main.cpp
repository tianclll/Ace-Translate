#include <QApplication>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QSharedMemory>
#include <QToolTip>
#include <QEvent>
#include <QTranslator>
#include <QJsonDocument>
#include <QJsonObject>
#include "mainwindow.h"
#include "splashscreen.h"

// 引擎初始化头文件
#include "docmind/core/GlobalEngineContext.hpp"

// 抑制 OpenCV libpng 刷屏警告（stderr 重定向）
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include <cstdlib>  // for _putenv_s
static int saved_stderr = -1;
static void mute_libpng() { saved_stderr = _dup(2); FILE* n; freopen_s(&n, "nul", "w", stderr); }
static void unmute_libpng() { if (saved_stderr >= 0) { _dup2(saved_stderr, 2); _close(saved_stderr); saved_stderr = -1; } }

extern QTranslator s_translator;

int main(int argc, char* argv[]) {
    // CPU 版：禁止 onnxruntime 加载 CUDA provider
#ifndef GGML_USE_CUDA
    _putenv_s("ORT_DISABLE_ALL_PROVIDERS", "1");
    _putenv_s("ORT_DISABLE_CUDA", "1");
#endif
    mute_libpng();
    QApplication app(argc, argv);
    app.setApplicationName("AceTranslatePro");
    app.setApplicationVersion("1.0.0");

    // 加载翻译（默认中文 = 不加载，Qt 直接显示 tr() 的源文本英文）
    // 后续可在设置中动态切换

    // 单实例限制：使用 QSharedMemory 检测是否已有实例运行
    QSharedMemory sharedMem("AceTranslatePro-Instance-Mutex");
    if (!sharedMem.create(1)) {
        QMessageBox::warning(nullptr,
            QApplication::translate("MainWindow", "Prompt"),
            QApplication::translate("MainWindow", "AceTranslatePro is already running."));
        return 0;
    }

    // 全局 ToolTip 过滤器：拦截所有 QEvent::ToolTip
    class ToolTipFilter : public QObject {
    public:
        using QObject::QObject;
    protected:
        bool eventFilter(QObject* obj, QEvent* event) override {
            if (event->type() == QEvent::ToolTip)
                return true;
            return QObject::eventFilter(obj, event);
        }
    };
    auto* toolTipFilter = new ToolTipFilter(&app);
    app.installEventFilter(toolTipFilter);

    // 禁用 QToolTip 字体
    QToolTip::setFont(QFont("", 1));

    // 设置应用图标
    app.setWindowIcon(QIcon(":/icons/LOGO.png"));

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

    // 在 SplashScreen 之前加载配置和翻译
    std::string uiLang = "zh_CN";
    {
        QFile cf(QCoreApplication::applicationDirPath() + "/config.json");
        if (cf.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(cf.readAll());
            if (doc.isObject()) {
                uiLang = doc.object().value("defaults").toObject().value("ui_language").toString("zh_CN").toStdString();
            }
        }
    }
    if (uiLang != "en_US") {
        QString qmFile = QStringLiteral(":/translations/") + QString::fromStdString(uiLang) + QStringLiteral(".qm");
        if (s_translator.load(qmFile)) {
            qApp->installTranslator(&s_translator);
        }
    }

    // 显示启动画面
    SplashScreen splash;
    splash.show();
    splash.setProgress(5, QApplication::translate("MainWindow", "Loading config…"));
    app.processEvents();

    // 在创建 MainWindow 之前先加载引擎（确保 config.json 已读取）
    try {
        splash.setProgress(15, QApplication::translate("MainWindow", "Loading engines…"));
        app.processEvents();
        docmind::GlobalEngineContext::getInstance().initialize(QCoreApplication::applicationDirPath().toStdString());
        splash.setProgress(70, QApplication::translate("MainWindow", "Engines loaded"));
        app.processEvents();
    } catch (const std::exception& e) {
        std::cerr << "Engine init failed: " << e.what() << std::endl;
        std::cerr.flush();
    } catch (...) {
        std::cerr << "Engine init failed with unknown error" << std::endl;
        std::cerr.flush();
    }

    splash.setProgress(85, QApplication::translate("MainWindow", "Initializing UI…"));
    app.processEvents();

    MainWindow w;
    splash.setProgress(100, QApplication::translate("MainWindow", "Startup complete"));
    app.processEvents();

    w.show();
    splash.finish(&w);

    return app.exec();
}
