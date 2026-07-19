#include "regioncapture.h"

#include <QPainter>
#include <QGuiApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFont>

// ============================================================
// RegionCapture — 全屏截图区域选择控件
// ============================================================

RegionCapture::RegionCapture(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
}

void RegionCapture::startCapture()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    fullScreenPixmap_ = screen->grabWindow(0);

    // Reset state
    selecting_ = false;
    startPoint_ = QPoint();
    endPoint_ = QPoint();
    capturedMat_ = cv::Mat();

    showFullScreen();
}

void RegionCapture::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 1. Draw the fullscreen screenshot as background
    painter.drawPixmap(0, 0, fullScreenPixmap_);

    // 2. Semi-transparent dark overlay over everything
    painter.fillRect(rect(), QColor(0, 0, 0, 128));

    if (selecting_) {
        QRect selectionRect(
            std::min(startPoint_.x(), endPoint_.x()),
            std::min(startPoint_.y(), endPoint_.y()),
            std::abs(endPoint_.x() - startPoint_.x()),
            std::abs(endPoint_.y() - startPoint_.y())
        );

        if (!selectionRect.isEmpty()) {
            // 3. Restore original colors inside the selected region
            painter.drawPixmap(selectionRect, fullScreenPixmap_, selectionRect);

            // 4. Dashed blue selection border
            QPen pen(QColor(0, 120, 255), 2, Qt::DashLine);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(selectionRect);

            // 5. Dimension text near the selection rectangle
            QString dimText = QStringLiteral("%1 x %2")
                .arg(selectionRect.width())
                .arg(selectionRect.height());
            painter.setPen(Qt::white);
            painter.setFont(QFont(QStringLiteral("Arial"), 12));
            painter.drawText(selectionRect.bottomRight() + QPoint(6, -4), dimText);
        }
    }
}

void RegionCapture::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        startPoint_ = event->pos();
        endPoint_ = startPoint_;
        selecting_ = true;
        update();
    }
}

void RegionCapture::mouseMoveEvent(QMouseEvent* event)
{
    if (selecting_) {
        endPoint_ = event->pos();
        update();
    }
}

void RegionCapture::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !selecting_)
        return;

    selecting_ = false;

    QRect selectionRect(
        std::min(startPoint_.x(), endPoint_.x()),
        std::min(startPoint_.y(), endPoint_.y()),
        std::abs(endPoint_.x() - startPoint_.x()),
        std::abs(endPoint_.y() - startPoint_.y())
    );

    // Cancel if the selected region is too small (< 10x10)
    if (selectionRect.width() < 10 || selectionRect.height() < 10) {
        update();
        return;
    }

    // Extract the selected region and convert to cv::Mat (BGR format)
    QPixmap regionPixmap = fullScreenPixmap_.copy(selectionRect);
    QImage regionImage = regionPixmap.toImage().convertToFormat(QImage::Format_BGR888);
    capturedMat_ = cv::Mat(
        regionImage.height(), regionImage.width(), CV_8UC3,
        const_cast<uchar*>(regionImage.bits()),
        regionImage.bytesPerLine()
    ).clone();

    hide();
    emit regionCaptured();
}

void RegionCapture::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        selecting_ = false;
        capturedMat_ = cv::Mat();
        hide();
    }
}
