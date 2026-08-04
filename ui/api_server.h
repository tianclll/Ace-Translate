#pragma once

#include <QObject>
#include <QHttpServer>
#include <QMutex>

#include "api_constants.h"
#include "api_job_tracker.h"

// Embedded HTTP API server.
// Runs entirely in the calling (main) thread — QHttpServer::listen() is
// non-blocking: it integrates with the Qt event loop and dispatches incoming
// requests to its own internal thread pool, so the GUI stays responsive and
// long-running translation jobs never block request handling.
class ApiServer : public QObject {
    Q_OBJECT
public:
    explicit ApiServer(QObject* parent = nullptr);
    ~ApiServer() override;

    bool start(int port);
    void stop();
    bool isRunning() const;
    int port() const;

signals:
    void started(int port);
    void stopped();
    void error(const QString& msg);

private:
    void registerRoutes();

    QHttpServer* server_ = nullptr;
    int port_ = 18888;
    JobTracker* tracker_ = nullptr;
    mutable QMutex mutex_;
    bool running_ = false;
};
