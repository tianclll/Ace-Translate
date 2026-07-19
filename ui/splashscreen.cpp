#include "splashscreen.h"
#include <QPainter>
#include <QFont>
#include <QApplication>

SplashScreen::SplashScreen()
    : QSplashScreen()
{
    // 创建一个 pixmap 作为启动画面背景
    QPixmap pix(500, 300);
    pix.fill(Qt::white);
    setPixmap(pix);

    // 使用 setMask 保持圆角效果
    // 标题
    auto* title = new QLabel(this);
    title->setPixmap(QPixmap(":/icons/LOGO.png").scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    title->move(226, 50);
    title->adjustSize();

    auto* titleText = new QLabel(QStringLiteral("AceTranslatePro"), this);
    titleText->setStyleSheet("font-size: 22px; font-weight: bold; color: #134E4A; background: transparent;");
    titleText->move(500 / 2 - 80, 110);
    titleText->adjustSize();

    // 进度条
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setFixedSize(360, 4);
    progressBar_->move(70, 180);
    progressBar_->setStyleSheet(
        "QProgressBar { border: none; background: #E8ECEF; border-radius: 2px; text-align: center; color: transparent; }"
        "QProgressBar::chunk { background: #0B7C72; border-radius: 2px; }");

    // 消息标签
    messageLabel_ = new QLabel(QStringLiteral("正在初始化…"), this);
    messageLabel_->setStyleSheet("font-size: 12px; color: #889096; background: transparent;");
    messageLabel_->move(500 / 2 - 50, 200);
    messageLabel_->adjustSize();

    // 版本号
    auto* verLabel = new QLabel(QStringLiteral("v1.0.0"), this);
    verLabel->setStyleSheet("font-size: 11px; color: #C4C8CC; background: transparent;");
    verLabel->move(500 / 2 - 15, 270);
    verLabel->adjustSize();
}

void SplashScreen::setProgress(int percent, const QString& message) {
    progressBar_->setValue(percent);
    messageLabel_->setText(message);
    messageLabel_->adjustSize();
    QApplication::processEvents();
}
