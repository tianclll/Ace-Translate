#include "toast.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QLabel>
#include <QDesktopServices>
#include <QUrl>
#include <QMouseEvent>

void ToastNotification::show(QWidget* parent, const QString& message, int durationMs,
                              const QColor& bgColor, const QString& iconPath,
                              const QString& actionText, const QString& actionUrl) {
    if (!parent) return;
    auto* toast = new ToastNotification(parent, message, durationMs, bgColor, iconPath,
                                         actionText, actionUrl);
    toast->raise();
    toast->QFrame::show();
    toast->animate();
}

ToastNotification::ToastNotification(QWidget* parent, const QString& message, int durationMs,
                                       const QColor& bgColor, const QString& iconPath,
                                       const QString& actionText, const QString& actionUrl)
    : QFrame(parent)
{
    setObjectName("toast");
    setAttribute(Qt::WA_DeleteOnClose);
    // WA_TransparentForMouseEvents 不能设了——需要点击事件
    auto* effect = new QGraphicsOpacityEffect;
    setGraphicsEffect(effect);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 16, 10);
    layout->setSpacing(6);

    // 图标（支持自定义路径，默认 yes.png）
    auto* icon = new QLabel;
    QString path = iconPath.isEmpty() ? QStringLiteral(":/icons/yes.png") : iconPath;
    QPixmap ico(path);
    if (!ico.isNull()) {
        icon->setPixmap(ico.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    icon->setFixedSize(20, 20);
    layout->addWidget(icon);

    label_ = new QLabel(message);
    label_->setStyleSheet("color: #FFFFFF; font-size: 13px; font-weight: 500; background: transparent;");
    label_->setWordWrap(false);
    layout->addWidget(label_, 1);

    // 动作按钮（如"打开设置"）
    if (!actionText.isEmpty() && !actionUrl.isEmpty()) {
        auto* actionLabel = new QLabel(QStringLiteral("<a href=\"%1\" style=\"color:#FFFFFF; text-decoration:underline;\">%2</a>")
                                        .arg(actionUrl, actionText));
        actionLabel->setStyleSheet("background: transparent;");
        actionLabel->setCursor(Qt::PointingHandCursor);
        connect(actionLabel, &QLabel::linkActivated, this, [actionUrl]() {
            QDesktopServices::openUrl(QUrl(actionUrl));
        });
        layout->addWidget(actionLabel);
    }

    // 背景色（默认绿色，参数可指定）
    setStyleSheet(QStringLiteral(
        "ToastNotification { background: %1; border: none; border-radius: 10px; }").arg(bgColor.name()));

    // 宽度自适应内容（图标 + spacing + 文字 + padding）
    adjustSize();
    int contentW = sizeHint().width();
    int w = qMin(contentW, parent->width() - 40);
    setFixedWidth(w);
    move(parent->width() / 2 - width() / 2, 16);

    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, [this]() {
        auto* anim = new QPropertyAnimation(this, "windowOpacity", this);
        anim->setDuration(500);
        anim->setStartValue(1.0);
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, this, &QWidget::close);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });
    timer_->start(durationMs);
}

void ToastNotification::animate() {
    // 淡入动画
    auto* fadeIn = new QPropertyAnimation(this, "windowOpacity", this);
    fadeIn->setDuration(300);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
}

void ToastNotification::setWindowOpacity(qreal opacity) {
    opacity_ = opacity;
    if (auto* effect = graphicsEffect()) {
        if (auto* op = qobject_cast<QGraphicsOpacityEffect*>(effect)) {
            op->setOpacity(opacity);
        }
    }
}
