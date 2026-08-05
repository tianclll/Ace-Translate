#include "api_server.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QMetaObject>
#include <QThread>
#include <QUuid>

#include "api_converters.h"
#include "api_constants.h"
#include "knowledgebase_manager.h"

#include "docmind/DocumentEngine.h"
#include "docmind/core/ConfigManager.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <algorithm>
#include <cstring>

using nlohmann::json;

// ============================================================
// Helpers
// ============================================================

static json qObjectToJson(const QHttpServerRequest& req) {
    QByteArray body = req.body();
    if (body.isEmpty()) return json::object();
    try {
        return json::parse(body.constData(), body.constData() + body.size());
    } catch (...) {
        return json::object();
    }
}

static inline bool hasKey(const nlohmann::json& j, const char* key) {
    return j.contains(key) && !j[key].is_null();
}

static QHttpServerResponse makeResponse(const nlohmann::json& body,
                                        QHttpServerResponse::StatusCode code)
{
    auto resp = QHttpServerResponse(
        QString::fromStdString(body.dump()),
        code
    );
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("Access-Control-Allow-Origin", "*");
    resp.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return resp;
}

static QHttpServerResponse makeOptionsResponse() {
    auto resp = QHttpServerResponse(QHttpServerResponse::StatusCode::Ok);
    resp.setHeader("Access-Control-Allow-Origin", "*");
    resp.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    resp.setHeader("Access-Control-Max-Age", "86400");
    return resp;
}

static QHttpServerResponse makeResponse(const nlohmann::json& body) {
    return makeResponse(body, QHttpServerResponse::StatusCode::Ok);
}

// ============================================================
// ApiServer — Implementation
// ============================================================

ApiServer::ApiServer(QObject* parent)
    : QObject(parent)
    , tracker_(JobTracker::instance())
{
}

ApiServer::~ApiServer() {
    stop();
}

bool ApiServer::start(int port) {
    if (isRunning()) return true;

    server_ = new QHttpServer(this);
    registerRoutes();

    quint16 actualPort = server_->listen(QHostAddress::Any, static_cast<quint16>(port));
    {
        QMutexLocker locker(&mutex_);
        if (actualPort > 0) {
            running_ = true;
            port_ = actualPort;
        } else {
            running_ = false;
            delete server_;
            server_ = nullptr;
        }
    }

    if (running_) {
        emit started(port_);
    } else {
        emit error(QStringLiteral("Failed to listen on port %1").arg(port));
    }
    return running_;
}

void ApiServer::stop() {
    {
        QMutexLocker locker(&mutex_);
        if (!running_) return;
        running_ = false;
    }

    if (server_) {
        delete server_;
        server_ = nullptr;
    }

    emit stopped();
}

bool ApiServer::isRunning() const {
    QMutexLocker locker(&mutex_);
    return running_;
}

int ApiServer::port() const {
    QMutexLocker locker(&mutex_);
    return port_;
}

// ============================================================
// Route registration
// ============================================================

void ApiServer::registerRoutes() {
    auto* srv = server_;

    // ---- Root ----
    srv->route("/", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest&) {
        // Serve the Web client HTML page
        QString htmlPath = QDir(QCoreApplication::applicationDirPath()).filePath("webapp/index.html");
        QFile f(htmlPath);
        if (!f.open(QIODevice::ReadOnly)) {
            auto resp = QHttpServerResponse(
                QStringLiteral("{\"status\":\"ok\",\"message\":\"AceTranslatePro REST API\"}"),
                QHttpServerResponse::StatusCode::Ok
            );
            resp.setHeader("Content-Type", "application/json");
            return resp;
        }
        QByteArray html = f.readAll();
        auto resp = QHttpServerResponse(html, QHttpServerResponse::StatusCode::Ok);
        resp.setHeader("Content-Type", "text/html; charset=utf-8");
        resp.setHeader("Access-Control-Allow-Origin", "*");
        return resp;
    });

    // ---- Health ----
    srv->route(ApiRoutes::kHealth, QHttpServerRequest::Method::Get, [](const QHttpServerRequest&) {
        return QHttpServerResponse(
            QStringLiteral("{\"status\":\"ok\"}"),
            QHttpServerResponse::StatusCode::Ok
        );
    });

    // ---- Status ----
    srv->route(ApiRoutes::kStatus, QHttpServerRequest::Method::Get, [this](const QHttpServerRequest&) {
        auto& km = KnowledgeBaseManager::getInstance();
        km.initialize();

        json engines = json::object();
        engines["translator"] = true;
        engines["ocr"] = true;
        engines["vlm"] = true;
        engines["asr"] = false;

        json body = {
            {"api_enabled",  isRunning()},
            {"port",         port()},
            {"engines",      engines},
            {"kb_doc_count", km.entryCount()}
        };
        return makeResponse(body);
    });

    // ---- Translate Text (async) ----
    srv->route(ApiRoutes::kTranslateText, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& req) {
            json body = qObjectToJson(req);
            if (!hasKey(body, ApiRoutes::BodyKeys::kText)) {
                return makeResponse({{"error", "Missing required field: text"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }
            QString jid = tracker_->submitJob(JobType::Text, body);
            json resp = {
                {ApiRoutes::RespKeys::kJobId, jid.toStdString()},
                {ApiRoutes::RespKeys::kStatus, "pending"},
                {ApiRoutes::RespKeys::kType, "text"},
                {ApiRoutes::RespKeys::kMessage, "Translation started"}
            };
            return makeResponse(resp, QHttpServerResponse::StatusCode::Accepted);
        }
    );

    // ---- Translate File (async) ----
    srv->route(ApiRoutes::kTranslateFile, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& req) {
            json body = qObjectToJson(req);
            if (!hasKey(body, ApiRoutes::BodyKeys::kFilePath)) {
                return makeResponse({{"error", "Missing required field: file_path"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }
            QString filePath = apiconv::qString(body[ApiRoutes::BodyKeys::kFilePath], "");
            if (filePath.isEmpty() || !QFileInfo(filePath).exists()) {
                return makeResponse({{"error", "File does not exist: " + filePath.toStdString()}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }
            QString jid = tracker_->submitJob(JobType::File, body);
            json resp = {
                {ApiRoutes::RespKeys::kJobId, jid.toStdString()},
                {ApiRoutes::RespKeys::kStatus, "pending"},
                {ApiRoutes::RespKeys::kType, "file"},
                {ApiRoutes::RespKeys::kMessage, "Translation started"}
            };
            return makeResponse(resp, QHttpServerResponse::StatusCode::Accepted);
        }
    );

    // ---- Translate Photo (async) ----
    srv->route(ApiRoutes::kTranslatePhoto, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& req) {
            json body = qObjectToJson(req);
            if (!hasKey(body, ApiRoutes::BodyKeys::kImageBase64)) {
                return makeResponse({{"error", "Missing required field: image_base64"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }
            if (body[ApiRoutes::BodyKeys::kImageBase64].get<std::string>().size() > 50 * 1024 * 1024) {
                return makeResponse({{"error", "Image data too large (max 50MB)"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }
            QString jid = tracker_->submitJob(JobType::Photo, body);
            json resp = {
                {ApiRoutes::RespKeys::kJobId, jid.toStdString()},
                {ApiRoutes::RespKeys::kStatus, "pending"},
                {ApiRoutes::RespKeys::kType, "photo"},
                {ApiRoutes::RespKeys::kMessage, "Translation started"}
            };
            return makeResponse(resp, QHttpServerResponse::StatusCode::Accepted);
        }
    );

    // ---- ASR Speech Recognition (sync) ----
    srv->route(ApiRoutes::kASRRecognize, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& req) {
            json body = qObjectToJson(req);
            if (!hasKey(body, ApiRoutes::BodyKeys::kAudioBase64)) {
                return makeResponse({{"error", "Missing required field: audio_base64"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            std::string b64 = body[ApiRoutes::BodyKeys::kAudioBase64].get<std::string>();
            if (b64.size() > 20 * 1024 * 1024) {
                return makeResponse({{"error", "Audio data too large (max 20MB)"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            // Decode base64
            QByteArray audioData = QByteArray::fromBase64(QByteArray(b64.c_str()));
            if (audioData.isEmpty()) {
                return makeResponse({{"error", "Invalid base64 audio data"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            // Strip WAV header if present: parse chunks to find data offset
            const short* pcm = reinterpret_cast<const short*>(audioData.constData());
            int pcmSamples = audioData.size() / sizeof(short);
            if (pcmSamples > 44 && audioData.startsWith("RIFF")) {
                int offset = 12;
                int dataOffset = -1;
                while (offset + 8 <= audioData.size()) {
                    const char* chunkId = audioData.constData() + offset;
                    quint32 chunkSize = (uchar)audioData[offset+4]
                        | ((uchar)audioData[offset+5] << 8)
                        | ((uchar)audioData[offset+6] << 16)
                        | ((uchar)audioData[offset+7] << 24);
                    if (memcmp(chunkId, "data", 4) == 0) {
                        dataOffset = offset + 8;
                        break;
                    }
                    offset += 8 + chunkSize;
                }
                if (dataOffset > 0) {
                    pcm = reinterpret_cast<const short*>(audioData.constData() + dataOffset);
                    pcmSamples = (audioData.size() - dataOffset) / sizeof(short);
                }
            }

            // Apply max_duration limit (default 10s, max 60s)
            int maxDuration = apiconv::qInt(body.value(ApiRoutes::BodyKeys::kMaxDuration, 10), 10);
            maxDuration = std::clamp(maxDuration, 1, 60);
            int maxSamples = maxDuration * 16000;
            if (pcmSamples > maxSamples) {
                pcmSamples = maxSamples;
            }

            if (pcmSamples <= 0) {
                return makeResponse({{"error", "No audio data after processing"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            // Resolve base_dir from config
            QString baseDir = QCoreApplication::applicationDirPath();
            auto& cfg = docmind::ConfigManager::getInstance();
            std::string baseDirStr = baseDir.toStdString();

            // Recognize
            std::string text;
            try {
                text = ::recognize_audio(pcm, pcmSamples, baseDirStr, false);
            } catch (const std::exception& e) {
                return makeResponse({{"error", std::string("ASR recognition failed: ") + e.what()}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            } catch (...) {
                return makeResponse({{"error", "ASR recognition failed: unknown error"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            int durationMs = (pcmSamples * 1000) / 16000;
            json resp = {
                {"text", text},
                {ApiRoutes::RespKeys::kDurationMs, durationMs},
                {"language", "auto"}
            };
            return makeResponse(resp);
        }
    );

    // ---- Get job by ID ----
    srv->route(QString("%1/%2").arg(ApiRoutes::kJobs, "<arg>"), QHttpServerRequest::Method::Get,
        [this](const QString& id) {
            ApiJob job = tracker_->getJob(id);
            if (job.id.isEmpty()) {
                return makeResponse({{"error", "Job not found"}},
                                    QHttpServerResponse::StatusCode::NotFound);
            }
            json j = {
                {ApiRoutes::RespKeys::kJobId,    job.id.toStdString()},
                {"type",      jobTypeToString(job.type)},
                {"status",    jobStatusToString(job.status)},
                {"created_at", job.createdAt.toStdString()},
                {"finished_at", job.finishedAt.toStdString()},
                {"result",    job.result},
                {"error",     job.error.toStdString()},
                {"params",    job.params}
            };
            return makeResponse(j);
        }
    );

    // ---- Cancel job ----
    srv->route(QString("%1/%2/cancel").arg(ApiRoutes::kJobs, "<arg>"), QHttpServerRequest::Method::Delete,
        [this](const QString& id) {
            ApiJob job = tracker_->getJob(id);
            if (job.id.isEmpty()) {
                return makeResponse({{"error", "Job not found"}},
                                    QHttpServerResponse::StatusCode::NotFound);
            }
            if (job.status != JobStatus::Pending) {
                return makeResponse(
                    {{"error", std::string("Job cannot be cancelled (status: ") +
                               jobStatusToString(job.status) + ")"}},
                    QHttpServerResponse::StatusCode::BadRequest);
            }
            tracker_->cancelJob(id);
            return makeResponse({{"job_id", id.toStdString()}, {"status", "cancelled"}});
        }
    );

    // ---- Knowledge Base: List entries ----
    srv->route(ApiRoutes::kKBEntries, QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest& req) {
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            int limit = 100, offset = 0;
            auto query = req.query();
            if (query.hasQueryItem(ApiRoutes::BodyKeys::kLimit))
                limit = std::clamp(query.queryItemValue(ApiRoutes::BodyKeys::kLimit).toInt(), 1, 500);
            if (query.hasQueryItem(ApiRoutes::BodyKeys::kOffset))
                offset = std::max(0, query.queryItemValue(ApiRoutes::BodyKeys::kOffset).toInt());

            auto entries = km.getAllEntries(limit, offset);
            json arr = json::array();
            for (const auto& e : entries) {
                arr.push_back(apiconv::entryToJson(e));
            }
            json body = {
                {ApiRoutes::RespKeys::kEntries, arr},
                {ApiRoutes::RespKeys::kTotal, km.entryCount()}
            };
            return makeResponse(body);
        }
    );

    // ---- Knowledge Base: Get single entry ----
    srv->route(ApiRoutes::kKBEntryDetail, QHttpServerRequest::Method::Get,
        [this](const QString& idStr) {
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            bool ok = false;
            int id = idStr.toInt(&ok);
            if (!ok || id <= 0) {
                return makeResponse({{"error", "Invalid entry ID"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            KnowledgeEntry entry = km.getEntry(id);
            if (entry.id < 0) {
                return makeResponse({{"error", "Entry not found"}},
                                    QHttpServerResponse::StatusCode::NotFound);
            }

            return makeResponse(apiconv::entryToJson(entry));
        }
    );

    // ---- Knowledge Base: Create entry ----
    srv->route(ApiRoutes::kKBEntries, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& req) {
            json body = qObjectToJson(req);
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            KnowledgeEntry entry;
            entry.title          = apiconv::qString(body.value(ApiRoutes::BodyKeys::kTitle, ""));
            entry.fileType       = apiconv::qString(body.value(ApiRoutes::BodyKeys::kFileType, ""));
            entry.sourcePath     = apiconv::qString(body.value(ApiRoutes::BodyKeys::kSourcePath, ""));
            entry.mdFilePath     = apiconv::qString(body.value(ApiRoutes::BodyKeys::kMdFilePath, ""));
            entry.translatedLang = apiconv::qString(body.value(ApiRoutes::BodyKeys::kTargetLangKB, ""));
            entry.fileSize       = apiconv::qInt64(body.value(ApiRoutes::BodyKeys::kFileSizeKB, 0));
            entry.markdownContent = apiconv::qString(body.value(ApiRoutes::BodyKeys::kMarkdownContent, ""));
            entry.assetsDir      = apiconv::qString(body.value(ApiRoutes::BodyKeys::kAssetsDirKB, ""));
            entry.parseStatus    = "done";

            int newId = -1;
            if (km.addEntry(entry, &newId) && newId > 0) {
                return makeResponse({{ApiRoutes::RespKeys::kId, newId}, {"status", "created"}},
                                    QHttpServerResponse::StatusCode::Created);
            }
            return makeResponse({{"error", "Failed to create entry"}},
                                QHttpServerResponse::StatusCode::InternalServerError);
        }
    );

    // ---- Knowledge Base: Delete single entry ----
    srv->route(ApiRoutes::kKBEntryDetail, QHttpServerRequest::Method::Delete,
        [this](const QString& idStr) {
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            bool ok = false;
            int id = idStr.toInt(&ok);
            if (!ok || id <= 0) {
                return makeResponse({{"error", "Invalid entry ID"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            if (km.deleteEntry(id)) {
                return makeResponse({{"deleted", id}});
            }
            return makeResponse({{"error", "Entry not found or delete failed"}},
                                QHttpServerResponse::StatusCode::NotFound);
        }
    );

    // ---- Knowledge Base: Batch delete ----
    srv->route(ApiRoutes::kKBEntries, QHttpServerRequest::Method::Delete,
        [this](const QHttpServerRequest& req) {
            json body = qObjectToJson(req);
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            if (!body.contains(ApiRoutes::BodyKeys::kIds) || !body[ApiRoutes::BodyKeys::kIds].is_array()) {
                return makeResponse({{"error", "Missing or invalid field: ids (array)"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            QList<int> ids;
            for (const auto& v : body[ApiRoutes::BodyKeys::kIds]) {
                if (v.is_number_integer()) ids.push_back(v.get<int>());
            }

            int deleted = km.deleteEntries(ids);
            return makeResponse({{"deleted_count", deleted}});
        }
    );

    // ---- Knowledge Base: Search ----
    srv->route(ApiRoutes::kKBSearch, QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest& req) {
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            QString keyword = req.query().queryItemValue(ApiRoutes::BodyKeys::kQ);
            auto results = km.searchEntries(keyword);
            json arr = json::array();
            for (const auto& e : results) {
                arr.push_back(apiconv::entryToJson(e));
            }
            return makeResponse({{ApiRoutes::RespKeys::kEntries, arr}});
        }
    );

    // ---- Knowledge Base: List tags ----
    srv->route(ApiRoutes::kKBTags, QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest&) {
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            auto tags = km.getAllTags();
            json arr = json::array();
            for (const auto& p : tags) {
                arr.push_back(apiconv::pairToJson(p));
            }
            return makeResponse({{"tags", arr}});
        }
    );

    // ---- Knowledge Base: Create tag ----
    srv->route(ApiRoutes::kKBTags, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& req) {
            json body = qObjectToJson(req);
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            QString name = apiconv::qString(body.value(ApiRoutes::BodyKeys::kName, ""));
            if (name.isEmpty()) {
                return makeResponse({{"error", "Missing required field: name"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            int tagId = km.addTag(name);
            if (tagId > 0) {
                return makeResponse({{ApiRoutes::RespKeys::kId, tagId},
                                     {"name", name.toStdString()}},
                                    QHttpServerResponse::StatusCode::Created);
            }
            return makeResponse({{"error", "Failed to create tag"}},
                                QHttpServerResponse::StatusCode::InternalServerError);
        }
    );

    // ---- Knowledge Base: Delete tag ----
    srv->route(QString("%1/%2").arg(ApiRoutes::kKBTags, "<arg>"), QHttpServerRequest::Method::Delete,
        [this](const QString& idStr) {
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            bool ok = false;
            int id = idStr.toInt(&ok);
            if (!ok || id <= 0) {
                return makeResponse({{"error", "Invalid tag ID"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            if (km.deleteTag(id)) {
                return makeResponse({{"deleted", id}});
            }
            return makeResponse({{"error", "Tag not found or delete failed"}},
                                QHttpServerResponse::StatusCode::NotFound);
        }
    );

    // ---- Knowledge Base: Glossary list ----
    srv->route(ApiRoutes::kKBGlossary, QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest& req) {
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            auto query = req.query();
            QString srcLang = query.queryItemValue(ApiRoutes::BodyKeys::kSourceLang);
            QString tgtLang = query.queryItemValue(ApiRoutes::BodyKeys::kTargetLang);

            // Build (term, translation) pairs for JSON output
            QList<QPair<QString, QString>> terms;
            if (!srcLang.isEmpty() && !tgtLang.isEmpty()) {
                terms = km.getGlossaryForLang(srcLang, tgtLang);
            } else {
                auto all = km.getAllGlossaryTerms();
                for (const auto& p : all) {
                    // getAllGlossaryTerms returns (id, "term → translation"); split it
                    QString combined = p.second;
                    int arrowPos = combined.indexOf(QStringLiteral(" → "));
                    if (arrowPos >= 0) {
                        terms.push_back(qMakePair(combined.left(arrowPos), combined.mid(arrowPos + 3)));
                    }
                }
            }

            json arr = json::array();
            for (const auto& p : terms) {
                arr.push_back(apiconv::glossaryToJson(p));
            }
            return makeResponse({{"terms", arr}});
        }
    );

    // ---- Knowledge Base: Add glossary term ----
    srv->route(ApiRoutes::kKBGlossary, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& req) {
            json body = qObjectToJson(req);
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            QString term        = apiconv::qString(body.value(ApiRoutes::BodyKeys::kTerm, ""));
            QString translation = apiconv::qString(body.value(ApiRoutes::BodyKeys::kTranslation, ""));
            QString srcLang     = apiconv::qString(body.value(ApiRoutes::BodyKeys::kSourceLang, ""));
            QString tgtLang     = apiconv::qString(body.value(ApiRoutes::BodyKeys::kTargetLang, ""));

            if (term.isEmpty() || translation.isEmpty()) {
                return makeResponse({{"error", "Missing required fields: term, translation"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            int termId = km.addGlossaryTerm(term, translation, srcLang, tgtLang);
            if (termId > 0) {
                json resp = {
                    {ApiRoutes::RespKeys::kId, termId},
                    {"term", term.toStdString()},
                    {"translation", translation.toStdString()},
                    {"source_lang", srcLang.toStdString()},
                    {"target_lang", tgtLang.toStdString()}
                };
                return makeResponse(resp, QHttpServerResponse::StatusCode::Created);
            }
            return makeResponse({{"error", "Failed to add glossary term"}},
                                QHttpServerResponse::StatusCode::InternalServerError);
        }
    );

    // ---- Knowledge Base: Delete glossary term ----
    srv->route(QString("%1/%2").arg(ApiRoutes::kKBGlossary, "<arg>"), QHttpServerRequest::Method::Delete,
        [this](const QString& idStr) {
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            bool ok = false;
            int id = idStr.toInt(&ok);
            if (!ok || id <= 0) {
                return makeResponse({{"error", "Invalid term ID"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            if (km.deleteGlossaryTerm(id)) {
                return makeResponse({{"deleted", id}});
            }
            return makeResponse({{"error", "Term not found or delete failed"}},
                                QHttpServerResponse::StatusCode::NotFound);
        }
    );

    // ---- Knowledge Base: Import file ----
    srv->route(ApiRoutes::kKBImport, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& req) {
            json body = qObjectToJson(req);
            auto& km = KnowledgeBaseManager::getInstance();
            km.initialize();
            if (!km.ensureDb()) {
                return makeResponse({{"error", "Failed to access knowledge base"}},
                                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            QString filePath = apiconv::qString(body.value(ApiRoutes::BodyKeys::kFilePath, ""));
            if (filePath.isEmpty() || !QFileInfo(filePath).exists()) {
                return makeResponse({{"error", "Invalid or missing file_path"}},
                                    QHttpServerResponse::StatusCode::BadRequest);
            }

            bool skipMd = apiconv::qBool(body.value(ApiRoutes::BodyKeys::kSkipMd, false), false);

            json fileBody = json::object();
            fileBody[ApiRoutes::BodyKeys::kFilePath] = filePath.toStdString();
            fileBody[ApiRoutes::BodyKeys::kTargetLanguage] = "Chinese";
            fileBody["_import"] = true;
            fileBody["skip_md"] = skipMd;

            QString jid = tracker_->submitJob(JobType::File, fileBody);
            json resp = {
                {ApiRoutes::RespKeys::kJobId, jid.toStdString()},
                {ApiRoutes::RespKeys::kStatus, "pending"},
                {ApiRoutes::RespKeys::kType, "import"},
                {ApiRoutes::RespKeys::kMessage, "Import started"}
            };
            return makeResponse(resp, QHttpServerResponse::StatusCode::Accepted);
        }
    );

    // ---- CORS: handle OPTIONS preflight ----
    srv->route("/*", QHttpServerRequest::Method::Options,
        [](const QHttpServerRequest&) {
            return makeOptionsResponse();
        }
    );
}
