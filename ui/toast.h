#pragma once

#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>
#include <QColor>

/**
 * @brief 顶部 Toast 通知组件
 * 从窗口顶部滑入，停留数秒后淡出消失，自动销毁
 */
class ToastNotification : public QFrame {
    Q_OBJECT
    Q_PROPERTY(qreal windowOpacity READ windowOpacity WRITE setWindowOpacity)
public:
    static void show(QWidget* parent, const QString& message, int durationMs = 3000,
                     const QColor& bgColor = QColor(52, 199, 89));

    qreal windowOpacity() const { return opacity_; }
    void setWindowOpacity(qreal opacity);

private:
    explicit ToastNotification(QWidget* parent, const QString& message, int durationMs,
                                const QColor& bgColor);
    void animate();

    QLabel* label_;
    QTimer* timer_;
    qreal opacity_ = 1.0;
};
