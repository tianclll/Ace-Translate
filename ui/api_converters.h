#pragma once

#include <QString>
#include <QList>
#include <QDateTime>
#include <QPair>
#include <nlohmann/json.hpp>

#include "knowledgebase_manager.h"  // for KnowledgeEntry

namespace apiconv {

// ==================== Qt → nlohmann::json ====================

inline nlohmann::json toJson(const QString& s) {
    return s.toStdString();
}

inline nlohmann::json toJson(const QList<QString>& list) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : list) arr.push_back(s.toStdString());
    return arr;
}

inline nlohmann::json toJson(const QList<int>& list) {
    nlohmann::json arr = nlohmann::json::array();
    for (int v : list) arr.push_back(v);
    return arr;
}

inline nlohmann::json toJson(const QDateTime& dt) {
    return dt.toString("yyyy-MM-dd HH:mm:ss").toStdString();
}

inline nlohmann::json toJson(const QByteArray& ba) {
    // Encode as base64 string for JSON compatibility
    return ba.toBase64().toStdString();
}

// KnowledgeEntry → JSON
inline nlohmann::json entryToJson(const KnowledgeEntry& e) {
    return {
        {"id",            e.id},
        {"title",         e.title.toStdString()},
        {"file_type",     e.fileType.toStdString()},
        {"source_path",   e.sourcePath.toStdString()},
        {"md_file_path",  e.mdFilePath.toStdString()},
        {"translated_lang", e.translatedLang.toStdString()},
        {"file_size",     e.fileSize},
        {"created_at",    toJson(e.createdAt)},
        {"summary",       e.summary.toStdString()},
        {"parse_status",  e.parseStatus.toStdString()},
        {"assets_dir",    e.assetsDir.toStdString()}
    };
}

// QPair<int, QString> → JSON (for tags)
inline nlohmann::json pairToJson(const QPair<int, QString>& p) {
    return {{"id", p.first}, {"name", p.second.toStdString()}};
}

// QPair<QString, QString> → JSON (for glossary: term, translation)
inline nlohmann::json glossaryToJson(const QPair<QString, QString>& p) {
    return {{"term", p.first.toStdString()}, {"translation", p.second.toStdString()}};
}

// ==================== nlohmann::json → Qt ====================

inline QString qString(const nlohmann::json& j, const QString& def = QString()) {
    if (j.is_string()) return QString::fromStdString(j.get<std::string>());
    if (j.is_null()) return def;
    return QString::fromStdString(j.dump());
}

inline int qInt(const nlohmann::json& j, int def = 0) {
    if (j.is_number_integer()) return j.get<int>();
    return def;
}

inline bool qBool(const nlohmann::json& j, bool def = false) {
    if (j.is_boolean()) return j.get<bool>();
    return def;
}

inline float qFloat(const nlohmann::json& j, float def = 0.0f) {
    if (j.is_number()) return static_cast<float>(j.get<double>());
    return def;
}

inline qint64 qInt64(const nlohmann::json& j, qint64 def = 0) {
    if (j.is_number_integer()) return j.get<qint64>();
    return def;
}

} // namespace apiconv
