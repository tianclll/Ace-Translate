#pragma once

#include <QObject>
#include <QMutex>
#include <QThreadPool>
#include <QDateTime>
#include <QAtomicInt>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

#include "api_constants.h"

// Forward declarations
struct ApiJob;
class JobTracker;

// Job type
enum class JobType {
    Text,
    File,
    Photo
};

// Job status
enum class JobStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
    Unknown
};

// Internal job record
struct ApiJob {
    QString id;
    JobType type;
    JobStatus status;
    QString createdAt;
    QString finishedAt;
    std::string result;       // translated text or output file path
    QString error;
    nlohmann::json params;    // original request body (for reference)
};

// Convert JobType to string (for JSON responses)
inline const char* jobTypeToString(JobType t) {
    switch (t) {
    case JobType::Text:   return "text";
    case JobType::File:   return "file";
    case JobType::Photo:  return "photo";
    }
    return "unknown";
}

// Convert JobStatus to string (for JSON responses)
inline const char* jobStatusToString(JobStatus s) {
    switch (s) {
    case JobStatus::Pending:   return "pending";
    case JobStatus::Running:   return "running";
    case JobStatus::Completed: return "completed";
    case JobStatus::Failed:    return "failed";
    case JobStatus::Cancelled: return "cancelled";
    }
    return "unknown";
}

// Convert string to JobStatus (for parsing)
inline JobStatus stringToJobStatus(const std::string& s) {
    if (s == "pending")   return JobStatus::Pending;
    if (s == "running")   return JobStatus::Running;
    if (s == "completed") return JobStatus::Completed;
    if (s == "failed")    return JobStatus::Failed;
    if (s == "cancelled") return JobStatus::Cancelled;
    return JobStatus::Unknown;
}

class JobTracker : public QObject {
    Q_OBJECT
public:
    static JobTracker* instance();
    static void destroy();

    // Submit a new job (thread-safe). Returns job ID.
    QString submitJob(JobType type, const nlohmann::json& params);

    // Get job by ID (thread-safe). Returns empty id if not found.
    ApiJob getJob(const QString& id) const;

    // Get all jobs (thread-safe).
    QList<ApiJob> getAllJobs() const;

    // Mark job as completed (called from worker thread via invokeMethod).
    void completeJob(const QString& id, const std::string& result, const QString& error = QString());

    // Cancel a pending job (best-effort; checks before executing).
    void cancelJob(const QString& id);

    // Prune old jobs, keeping at most keepCount.
    void pruneJobs(int keepCount = 100);

signals:
    void jobCompleted(const QString& jobId);

private:
    JobTracker();
    ~JobTracker() override;
    JobTracker(const JobTracker&) = delete;
    JobTracker& operator=(const JobTracker&) = delete;

    mutable QMutex mutex_;
    std::vector<std::unique_ptr<ApiJob>> jobs_;
    QAtomicInt counter_{0};
    QThreadPool* pool_;

    static JobTracker* s_instance;
    static std::once_flag s_initFlag;
};
