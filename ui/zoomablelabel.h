#pragma once

#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QPoint>
#include <cmath>

class ZoomableLabel : public QLabel {
    Q_OBJECT
public:
    explicit ZoomableLabel(QWidget* parent = nullptr)
        : QLabel(parent), zoom_(1.0), minZoom_(0.3), maxZoom_(8.0) {
        setAlignment(Qt::AlignCenter);
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setOriginalPixmap(const QPixmap& pixmap) {
        originalFullPixmap_ = pixmap;
        zoom_ = 1.0;
        updateZoom();
    }

    // 设置原始全分辨率图（用于缩放），同时显示适配视图的缩略图
    void setFullImage(const QPixmap& fullPix, const QPixmap& displayPix) {
        originalFullPixmap_ = fullPix;
        // zoom_ 设为实际显示比例（缩略图相对全分辨率），避免 resizeEvent 跳回全尺寸
        zoom_ = (double)displayPix.width() / fullPix.width();
        setPixmap(displayPix);
        setFixedSize(displayPix.size());
    }

    void clearImage() {
        originalFullPixmap_ = QPixmap();
        zoom_ = 1.0;
        QLabel::clear();
        // 松开 fixedSize，让布局/scrollarea 控制大小
        setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        setMinimumSize(200, 140);
        updateGeometry();
    }

    bool hasImage() const { return !originalFullPixmap_.isNull(); }
    double zoomLevel() const { return zoom_; }
    const QPixmap& originalFullPixmap() const { return originalFullPixmap_; }

signals:
    void zoomChanged(double zoom);

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (originalFullPixmap_.isNull()) { QLabel::wheelEvent(event); return; }
        double delta = event->angleDelta().y();
        double factor = (delta > 0) ? 1.15 : 1.0 / 1.15;
        double newZoom = std::max(minZoom_, std::min(maxZoom_, zoom_ * factor));
        if (newZoom != zoom_) {
            zoom_ = newZoom;
            updateZoom();
            emit zoomChanged(zoom_);
        }
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (originalFullPixmap_.isNull()) { QLabel::mousePressEvent(event); return; }
        if (event->button() == Qt::LeftButton) {
            dragging_ = true;
            dragStartPos_ = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (dragging_ && (event->buttons() & Qt::LeftButton)) {
            QPoint delta = event->pos() - dragStartPos_;
            dragStartPos_ = event->pos();
            QWidget* p = parentWidget();
            while (p) {
                auto* sa = qobject_cast<QScrollArea*>(p);
                if (sa) {
                    if (auto* hb = sa->horizontalScrollBar()) hb->setValue(hb->value() - delta.x());
                    if (auto* vb = sa->verticalScrollBar()) vb->setValue(vb->value() - delta.y());
                    break;
                }
                p = p->parentWidget();
            }
            event->accept();
            return;
        }
        setCursor(dragging_ ? Qt::ClosedHandCursor : (originalFullPixmap_.isNull() ? Qt::ArrowCursor : Qt::OpenHandCursor));
        QLabel::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && dragging_) {
            dragging_ = false;
            setCursor(Qt::OpenHandCursor);
            event->accept();
        }
        QLabel::mouseReleaseEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        if (!originalFullPixmap_.isNull() && zoom_ == 1.0 && width() > 1 && height() > 1)
            updateZoom();
    }

private:
    void updateZoom() {
        if (originalFullPixmap_.isNull()) return;
        QSize baseSize = originalFullPixmap_.size();
        QSize newSize(qMax(1, (int)(baseSize.width() * zoom_)), qMax(1, (int)(baseSize.height() * zoom_)));
        setPixmap(originalFullPixmap_.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        setFixedSize(newSize);
    }

    QPixmap originalFullPixmap_;
    double zoom_ = 1.0;
    double minZoom_;
    double maxZoom_;
    bool dragging_ = false;
    QPoint dragStartPos_;
};
