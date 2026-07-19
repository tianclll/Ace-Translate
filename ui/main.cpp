#include <QApplication>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QSharedMemory>
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

    // 单实例限制：使用 QSharedMemory 检测是否已有实例运行
    QSharedMemory sharedMem("AceTranslatePro-Instance-Mutex");
    if (!sharedMem.create(1)) {
        QMessageBox::warning(nullptr, QStringLiteral("提示"),
                             QStringLiteral("AceTranslatePro 已在运行中，请勿重复启动。"));
        return 0;
    }

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

    // 显示启动画面
    SplashScreen splash;
    splash.show();
    splash.setProgress(5, QStringLiteral("正在加载配置…"));
    app.processEvents();

    // 在创建 MainWindow 之前先加载引擎（确保 config.json 已读取）
    try {
        splash.setProgress(15, QStringLiteral("正在加载引擎…"));
        app.processEvents();
        docmind::GlobalEngineContext::getInstance().initialize();
        splash.setProgress(70, QStringLiteral("引擎加载完成"));
        app.processEvents();
    } catch (const std::exception& e) {
        qWarning() << "Engine init failed:" << e.what();
    } catch (...) {
        qWarning() << "Engine init failed with unknown error";
    }

    splash.setProgress(85, QStringLiteral("正在初始化界面…"));
    app.processEvents();

    MainWindow w;
    splash.setProgress(100, QStringLiteral("启动完成"));
    app.processEvents();

    w.show();
    splash.finish(&w);

    return app.exec();
}
