#pragma once

#include <QSplashScreen>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

class SplashScreen : public QSplashScreen {
    Q_OBJECT
public:
    explicit SplashScreen();
    void setProgress(int percent, const QString& message);

private:
    QProgressBar* progressBar_;
    QLabel* messageLabel_;
};
