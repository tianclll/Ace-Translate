#pragma once

#include <QWidget>
#include <QPoint>
#include <opencv2/opencv.hpp>

class RegionCapture : public QWidget {
    Q_OBJECT
public:
    explicit RegionCapture(QWidget* parent = nullptr);
    ~RegionCapture() override = default;

    void startCapture();
    cv::Mat capturedImage() const { return capturedMat_; }

signals:
    void regionCaptured();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPixmap fullScreenPixmap_;
    QPoint startPoint_;
    QPoint endPoint_;
    bool selecting_ = false;
    cv::Mat capturedMat_;
};
