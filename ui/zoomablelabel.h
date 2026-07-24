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
    void setAutoFitEnabled(bool enable) { autoFit_ = enable; }
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

    void setFullImage(const QPixmap& fullPix, const QPixmap& displayPix) {
        originalFullPixmap_ = fullPix;
        zoom_ = (double)displayPix.width() / fullPix.width();
        autoFit_ = true;
        setPixmap(displayPix);
        updateGeometry();
    }

    void clearImage() {
        originalFullPixmap_ = QPixmap();
        zoom_ = 1.0;
        autoFit_ = false;
        QLabel::clear();
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
            autoFit_ = false;
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
        // 窗口调整大小时，视口大小变化，ZoomableLabel 自适应
        if (autoFit_ && !originalFullPixmap_.isNull() && width() > 50 && height() > 50) {
            double newZoom = qMin((double)width() / originalFullPixmap_.width(),
                                  (double)height() / originalFullPixmap_.height());
            if (std::abs(newZoom - zoom_) / std::max(newZoom, zoom_) > 0.02) {
                zoom_ = newZoom;
                updateZoom();
            }
        }
    }

    QSize sizeHint() const override {
        if (!pixmap().isNull())
            return pixmap().size(); // 尺寸由 QScrollArea 通过布局控制
        return QSize(360, 240);
    }

private:
    void updateZoom() {
        if (originalFullPixmap_.isNull()) return;
        QSize baseSize = originalFullPixmap_.size();
        QSize newSize(qMax(1, (int)(baseSize.width() * zoom_)), qMax(1, (int)(baseSize.height() * zoom_)));
        setPixmap(originalFullPixmap_.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        // ★ 核心：彻底移除 setFixedSize(newSize)，让 QScrollArea 直接管理它的物理大小，
        // 彻底切断因外层样式变化导致图片强行布局闪烁的源头！
    }

    QPixmap originalFullPixmap_;
    double zoom_ = 1.0;
    double minZoom_;
    double maxZoom_;
    bool dragging_ = false;
    bool autoFit_ = false;
    QPoint dragStartPos_;
};