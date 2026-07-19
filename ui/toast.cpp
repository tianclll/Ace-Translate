#include "toast.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QLabel>

void ToastNotification::show(QWidget* parent, const QString& message, int durationMs,
                              const QColor& bgColor) {
    if (!parent) return;
    auto* toast = new ToastNotification(parent, message, durationMs, bgColor);
    toast->raise();
    toast->QFrame::show();
    toast->animate();
}

ToastNotification::ToastNotification(QWidget* parent, const QString& message, int durationMs,
                                       const QColor& bgColor)
    : QFrame(parent)
{
    setObjectName("toast");
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* effect = new QGraphicsOpacityEffect;
    setGraphicsEffect(effect);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 16, 10);
    layout->setSpacing(6);

    // 图标
    auto* icon = new QLabel;
    QPixmap yesPix(QStringLiteral(":/icons/yes.png"));
    if (!yesPix.isNull()) {
        icon->setPixmap(yesPix.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    icon->setFixedSize(20, 20);
    layout->addWidget(icon);

    label_ = new QLabel(message);
    label_->setStyleSheet("color: #FFFFFF; font-size: 13px; font-weight: 500; background: transparent;");
    label_->setWordWrap(false);
    layout->addWidget(label_);

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
