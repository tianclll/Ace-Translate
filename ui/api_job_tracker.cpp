#include "api_job_tracker.h"

#include <QDir>
#include <QTemporaryFile>
#include <QUuid>
#include <QThreadPool>
#include <QMetaObject>

#include "api_converters.h"

#include "docmind/DocumentEngine.h"
#include "docmind/modules/TextTranslationModule.hpp"
#include "docmind/modules/PhotoTranslationModule.hpp"

#include <opencv2/opencv.hpp>

#include <cstdlib>

// ============================================================
// ApiJobWorker — 在 QThreadPool 中执行的翻译任务
// ============================================================
class ApiJobWorker : public QRunnable {
public:
    ApiJobWorker(JobTracker* tracker, const QString& jobId,
                 JobType type, const nlohmann::json& params)
        : tracker_(tracker), jobId_(jobId), type_(type), params_(params) {}

    void run() override {
        std::string result;
        QString error;

        try {
            switch (type_) {
            case JobType::Text: {
                std::string text = params_.value("text", "");
                std::string lang  = params_.value("target_language", "English");
                int maxTokens     = params_.value("max_tokens", 512);
                if (text.empty()) {
                    error = QStringLiteral("Missing required field: text");
                } else {
                    result = ::translate_text(text, lang, maxTokens);
                }
                break;
            }
            case JobType::File: {
                std::string path    = params_.value("file_path", "");
                std::string lang    = params_.value("target_language", "English");
                std::string outPath = params_.value("output_path", "");
                float threshold     = apiconv::qFloat(params_.value("layout_threshold", 0.5f), 0.5f);
                int dpi             = apiconv::qInt(params_.value("pdf_dpi", 200), 200);
                bool warp           = apiconv::qBool(params_.value("enable_warp", true), true);
                bool enhance        = apiconv::qBool(params_.value("enable_enhance", false), false);
                if (path.empty()) {
                    error = QStringLiteral("Missing required field: file_path");
                } else {
                    result = ::process_file(path, outPath, "", lang, threshold, dpi, warp, enhance);
                }
                break;
            }
            case JobType::Photo: {
                std::string b64     = params_.value("image_base64", "");
                std::string lang    = params_.value("target_language", "English");
                int maxTokens       = apiconv::qInt(params_.value("max_tokens", 512), 512);
                if (b64.empty()) {
                    error = QStringLiteral("Missing required field: image_base64");
                } else {
                    // Decode base64 → temp file → process_photo
                    QByteArray imgData = QByteArray::fromBase64(QByteArray(b64.c_str()));
                    if (imgData.isEmpty()) {
                        error = QStringLiteral("Invalid base64 image data");
                    } else {
                        QString tmpPath = saveTempImage(imgData);
                        if (tmpPath.isEmpty()) {
                            error = QStringLiteral("Failed to save temporary image");
                        } else {
                            result = ::process_photo(tmpPath.toStdString(), "", "", lang, maxTokens);
                            QFile::remove(tmpPath);
                        }
                    }
                }
                break;
            }
            }
        } catch (const std::exception& e) {
            error = QString::fromUtf8(e.what());
        } catch (...) {
            error = QStringLiteral("Unknown error during translation");
        }

        // Report completion back to JobTracker on its thread
        QMetaObject::invokeMethod(tracker_, [tracker = tracker_, id = jobId_,
                                             result, error]() mutable {
            tracker->completeJob(id, result, error);
        }, Qt::QueuedConnection);
    }

private:
    JobTracker* tracker_;
    QString jobId_;
    JobType type_;
    nlohmann::json params_;

    // Save raw image bytes to a temp file under the app's temp directory.
    // Returns the path on success, empty string on failure.
    static QString saveTempImage(const QByteArray& data) {
        QString tmpDir = QDir::temp().absoluteFilePath("acetranslate_photos");
        QDir().mkpath(tmpDir);
        QString tmpPath = tmpDir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".jpg";
        QFile f(tmpPath);
        if (!f.open(QIODevice::WriteOnly)) return QString();
        f.write(data);
        f.close();
        return tmpPath;
    }
};

// ============================================================
// JobTracker — 单例实现
// ============================================================

JobTracker* JobTracker::s_instance = nullptr;
std::once_flag JobTracker::s_initFlag;

JobTracker* JobTracker::instance() {
    std::call_once(s_initFlag, []() { s_instance = new JobTracker; });
    return s_instance;
}

void JobTracker::destroy() {
    delete s_instance;
    s_instance = nullptr;
}

JobTracker::JobTracker()
    : pool_(QThreadPool::globalInstance())
{
    pool_->setMaxThreadCount(4);
}

JobTracker::~JobTracker() = default;

QString JobTracker::submitJob(JobType type, const nlohmann::json& params) {
    QString id = QStringLiteral("job-%1").arg(counter_.fetchAndAddOrdered(1) + 1, 6, 10, QLatin1Char('0'));

    ApiJob job;
    job.id = id;
    job.type = type;
    job.status = JobStatus::Pending;
    job.createdAt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    job.finishedAt.clear();
    job.result.clear();
    job.error.clear();
    job.params = params;

    {
        QMutexLocker locker(&mutex_);
        jobs_.push_back(std::make_unique<ApiJob>(job));
    }

    // Submit to thread pool
    auto* worker = new ApiJobWorker(this, id, type, params);
    worker->setAutoDelete(true);
    pool_->start(worker);

    return id;
}

ApiJob JobTracker::getJob(const QString& id) const {
    QMutexLocker locker(&mutex_);
    for (const auto& jp : jobs_) {
        if (jp->id == id) {
            ApiJob copy;
            copy.id = jp->id;
            copy.type = jp->type;
            copy.status = jp->status;
            copy.createdAt = jp->createdAt;
            copy.finishedAt = jp->finishedAt;
            copy.result = jp->result;
            copy.error = jp->error;
            copy.params = jp->params;
            return copy;
        }
    }
    return ApiJob{};
}

QList<ApiJob> JobTracker::getAllJobs() const {
    QMutexLocker locker(&mutex_);
    QList<ApiJob> result;
    result.reserve(jobs_.size());
    for (const auto& jp : jobs_) {
        ApiJob copy;
        copy.id = jp->id;
        copy.type = jp->type;
        copy.status = jp->status;
        copy.createdAt = jp->createdAt;
        copy.finishedAt = jp->finishedAt;
        copy.result = jp->result;
        copy.error = jp->error;
        copy.params = jp->params;
        result.push_back(copy);
    }
    return result;
}

void JobTracker::completeJob(const QString& id, const std::string& result, const QString& error) {
    QMutexLocker locker(&mutex_);
    for (const auto& jp : jobs_) {
        if (jp->id == id && jp->status != JobStatus::Cancelled) {
            jp->status = error.isEmpty() ? JobStatus::Completed : JobStatus::Failed;
            jp->finishedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
            jp->result = result;
            jp->error = error;
            emit jobCompleted(id);
            return;
        }
    }
}

void JobTracker::cancelJob(const QString& id) {
    QMutexLocker locker(&mutex_);
    for (const auto& jp : jobs_) {
        if (jp->id == id && jp->status == JobStatus::Pending) {
            jp->status = JobStatus::Cancelled;
            jp->finishedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
            jp->error = QStringLiteral("Cancelled by user");
            emit jobCompleted(id);
            return;
        }
    }
}

void JobTracker::pruneJobs(int keepCount) {
    QMutexLocker locker(&mutex_);
    while (jobs_.size() > static_cast<size_t>(keepCount)) {
        jobs_.erase(jobs_.begin());
    }
}
