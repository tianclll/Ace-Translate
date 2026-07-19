#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QPoint>

class FloatTranslateWindow : public QWidget {
    Q_OBJECT
public:
    explicit FloatTranslateWindow(QWidget* parent = nullptr);
    ~FloatTranslateWindow() override;

    void setStayOnTop(bool on);
    void translateText(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onTranslate();
    void onCopyResult();
    void onToggleLock();

signals:
    void translationDone(const QString& source, const QString& result);

private:
    void performTranslate(const QString& text);

    QTextEdit* sourceText_;
    QTextEdit* resultText_;
    QComboBox* langCombo_;
    QPushButton* translateBtn_;
    QPushButton* copyBtn_;
    QPushButton* lockBtn_;

    bool locked_ = false;
    bool dragging_ = false;
    QPoint dragStartPos_;
};
