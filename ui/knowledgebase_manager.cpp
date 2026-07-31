#include "knowledgebase_manager.h"
#include "docmind/core/ConfigManager.hpp"
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QTextStream>
#include <QRegularExpression>
#include <string>
#include <windows.h>

KnowledgeBaseManager& KnowledgeBaseManager::getInstance() {
    static KnowledgeBaseManager inst;
    return inst;
}

KnowledgeBaseManager::KnowledgeBaseManager(QObject* parent)
    : QObject(parent) {}

KnowledgeBaseManager::~KnowledgeBaseManager() {
    if (db_.isOpen())
        db_.close();
}

QString KnowledgeBaseManager::storagePath() const {
    std::string customPath = docmind::ConfigManager::getInstance()
                                 .getNestedJson("defaults")
                                 .value("kb_storage_path", nlohmann::json(""))
                                 .get<std::string>();
    if (!customPath.empty()) {
        QString qpath = QString::fromStdString(customPath);
        if (!qpath.endsWith('/') && !qpath.endsWith('\\'))
            qpath += '/';
        return qpath;
    }
    return QCoreApplication::applicationDirPath() + "/knowledge_base/";
}

// 每次操作前确保数据库连接打开
bool KnowledgeBaseManager::ensureDb() {
    if (!initialized_) {
        initialize();
    }
    if (!db_.isOpen()) {
        db_ = QSqlDatabase::database("kb_conn");
        if (!db_.isOpen()) {
            if (!db_.open()) {
                qWarning() << "[KB] Failed to reopen database:" << db_.lastError().text();
                return false;
            }
        }
    }
    return true;
}

bool KnowledgeBaseManager::initialize(const QString& dbPath) {
    if (initialized_) return true;

    QString path = dbPath.isEmpty() ? (storagePath() + "knowledge.db") : dbPath;
    QDir().mkpath(QFileInfo(path).absolutePath());

    // 使用命名连接避免与其他 SQLite 连接冲突
    if (!QSqlDatabase::contains("kb_conn")) {
        db_ = QSqlDatabase::addDatabase("QSQLITE", "kb_conn");
    } else {
        db_ = QSqlDatabase::database("kb_conn");
        if (db_.isOpen()) {
            initialized_ = true;
            return true;
        }
    }

    db_.setDatabaseName(path);
    if (!db_.open()) {
        return false;
    }

    dbPath_ = path;
    initialized_ = createTables();
    return initialized_;
}

bool KnowledgeBaseManager::createTables() {
    QSqlQuery query(db_);
    // documents 表
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS documents (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            title      TEXT NOT NULL,
            file_type  TEXT NOT NULL DEFAULT '',
            source_path TEXT DEFAULT '',
            md_path    TEXT NOT NULL,
            lang       TEXT DEFAULT '',
            file_size  INTEGER DEFAULT 0,
            created_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))
        )
    )";
    if (!query.exec(sql)) {
        qWarning() << "[KB] Create documents table failed:" << query.lastError().text();
        return false;
    }

    // tags 表
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS tags (
            id   INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE
        )
    )");

    // document_tags 关联表
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS document_tags (
            doc_id INTEGER NOT NULL,
            tag_id INTEGER NOT NULL,
            PRIMARY KEY (doc_id, tag_id),
            FOREIGN KEY (doc_id) REFERENCES documents(id) ON DELETE CASCADE,
            FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
        )
    )");

    // 兼容已有数据库：添加新列（忽略列已存在错误）
    query.exec("ALTER TABLE documents ADD COLUMN summary TEXT DEFAULT ''");
    query.exec("ALTER TABLE documents ADD COLUMN parse_status TEXT DEFAULT 'pending'");

    return true;
}

bool KnowledgeBaseManager::addEntry(const KnowledgeEntry& entry, int* outId) {
    if (!ensureDb()) return false;

    // 先插入占位行获取 ID
    QSqlQuery query(db_);
    if (!query.exec("INSERT INTO documents (title, file_type, source_path, md_path, lang, file_size) "
                    "VALUES ('', '', '', '', '', 0)")) {
        QString err = QStringLiteral("[KB] INSERT failed: %1").arg(query.lastError().text());
        MessageBoxA(nullptr, err.toUtf8().constData(), "KB Debug", MB_OK);
        return false;
    }
    int newId = query.lastInsertId().toInt();
    if (newId <= 0) {
        MessageBoxA(nullptr, ("[KB] Invalid lastInsertId: " + QString::number(newId)).toUtf8().constData(), "KB Debug", MB_OK);
        return false;
    }

    // 写入 .md 文件（用文件名做文件名，避免冲突）
    QString mdDir = storagePath() + "md/";
    QDir().mkpath(mdDir);
    QString safeName = entry.title;
    safeName.replace(QRegularExpression("[^a-zA-Z0-9_一-鿿\\.-]"), "_");
    if (safeName.isEmpty() || safeName.length() > 80)
        safeName = QString::number(newId);
    QString mdFilePath = mdDir + safeName + ".md";
    // 尝试创建 .md 文件并用 QFile::exists 验证
    QFile file(mdFilePath);
    bool fileOk = file.open(QIODevice::WriteOnly | QIODevice::Text);
    if (!fileOk) {
        QString errMsg = QStringLiteral("[KB] FAIL\nmdDir: %1\nmdFile: %2\nerror: %3")
            .arg(mdDir, mdFilePath, file.errorString());
        MessageBoxA(nullptr, errMsg.toUtf8().constData(), "KB Debug", MB_OK);
        // 回滚
        QSqlQuery del(db_);
        del.prepare("DELETE FROM documents WHERE id = :id");
        del.bindValue(":id", newId);
        del.exec();
        return false;
    }
    QTextStream out(&file);
    out << entry.markdownContent;
    file.close();

    // 更新真实数据
    QSqlQuery update(db_);
    update.prepare("UPDATE documents SET title=:t, file_type=:ft, source_path=:sp, "
                   "md_path=:mp, lang=:l, file_size=:fs WHERE id=:id");
    update.bindValue(":t",   entry.title);
    update.bindValue(":ft",  entry.fileType);
    update.bindValue(":sp",  entry.sourcePath);
    update.bindValue(":mp",  QString("md/%1.md").arg(safeName));
    update.bindValue(":l",   entry.translatedLang);
    update.bindValue(":fs",  entry.fileSize);
    update.bindValue(":id",  newId);

    if (!update.exec()) {
        qWarning() << "[KB] UPDATE entry failed:" << update.lastError().text();
        QFile::remove(mdFilePath);
        QSqlQuery del(db_);
        del.prepare("DELETE FROM documents WHERE id = :id");
        del.bindValue(":id", newId);
        del.exec();
        return false;
    }

    if (outId) *outId = newId;
    return true;
}

bool KnowledgeBaseManager::deleteEntry(int id) {
    if (!ensureDb()) return false;

    // 先查询 md_path
    QSqlQuery q(db_);
    q.prepare("SELECT md_path FROM documents WHERE id = :id");
    q.bindValue(":id", id);
    QString mdPath;
    if (q.exec() && q.next()) {
        mdPath = q.value(0).toString();
    }

    // 删除 .md 文件
    if (!mdPath.isEmpty()) {
        QFile::remove(storagePath() + mdPath);
    }

    // 删除 DB 行
    QSqlQuery query(db_);
    query.prepare("DELETE FROM documents WHERE id = :id");
    query.bindValue(":id", id);
    return query.exec();
}

int KnowledgeBaseManager::deleteEntries(const QList<int>& ids) {
    if (!initialized_ || ids.isEmpty()) return 0;

    if (!db_.isOpen()) {
        qWarning() << "[KB] DB not open, reopening...";
        if (!db_.open()) return 0;
    }

    // 查询所有 md_path
    QStringList idStrs;
    for (int id : ids) idStrs << QString::number(id);
    QSqlQuery q(db_);
    q.exec("SELECT id, md_path FROM documents WHERE id IN (" + idStrs.join(",") + ")");
    QStringList mdPaths;
    while (q.next()) {
        mdPaths.append(q.value(1).toString());
    }

    // 删除 .md 文件
    for (const QString& mdPath : mdPaths) {
        if (!mdPath.isEmpty()) {
            QFile::remove(storagePath() + mdPath);
        }
    }

    // 一次 SQL 删除所有文档
    QSqlQuery query(db_);
    if (!query.prepare("DELETE FROM documents WHERE id IN (" + idStrs.join(",") + ")")) {
        qWarning() << "[KB] deleteEntries prepare failed:" << query.lastError().text();
        return 0;
    }
    if (!query.exec()) {
        qWarning() << "[KB] deleteEntries exec failed:" << query.lastError().text();
        return 0;
    }
    return query.numRowsAffected();
}

QList<KnowledgeEntry> KnowledgeBaseManager::getAllEntries(int limit, int offset) {
    QList<KnowledgeEntry> list;
    if (!initialized_) return list;

    QSqlQuery query(db_);
    query.prepare("SELECT id, title, file_type, source_path, md_path, lang, file_size, summary, created_at "
                  "FROM documents ORDER BY id DESC LIMIT :lim OFFSET :off");
    query.bindValue(":lim", limit);
    query.bindValue(":off", offset);
    if (!query.exec()) return list;

    while (query.next()) {
        KnowledgeEntry e;
        e.id             = query.value(0).toInt();
        e.title          = query.value(1).toString();
        e.fileType       = query.value(2).toString();
        e.sourcePath     = query.value(3).toString();
        e.mdFilePath     = query.value(4).toString();
        e.translatedLang = query.value(5).toString();
        e.fileSize       = query.value(6).toLongLong();
        e.summary        = query.value(7).toString();
        e.createdAt      = QDateTime::fromString(query.value(8).toString(), "yyyy-MM-dd hh:mm:ss");
        list.append(e);
    }
    return list;
}

KnowledgeEntry KnowledgeBaseManager::getEntry(int id) {
    KnowledgeEntry e;
    if (!initialized_) return e;

    QSqlQuery query(db_);
    query.prepare("SELECT id, title, file_type, source_path, md_path, lang, file_size, summary, created_at "
                  "FROM documents WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec() || !query.next()) return e;

    e.id             = query.value(0).toInt();
    e.title          = query.value(1).toString();
    e.fileType       = query.value(2).toString();
    e.sourcePath     = query.value(3).toString();
    e.mdFilePath     = query.value(4).toString();
    e.translatedLang = query.value(5).toString();
    e.fileSize       = query.value(6).toLongLong();
    e.summary        = query.value(7).toString();
    e.createdAt      = QDateTime::fromString(query.value(8).toString(), "yyyy-MM-dd hh:mm:ss");

    // 读取 .md 文件内容
    QString fullPath = storagePath() + e.mdFilePath;
    QFile f(fullPath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        e.markdownContent = in.readAll();
        f.close();
    }
    return e;
}

int KnowledgeBaseManager::entryCount() const {
    if (!initialized_) return 0;
    QSqlQuery query(db_);
    query.exec("SELECT COUNT(*) FROM documents");
    return query.next() ? query.value(0).toInt() : 0;
}

bool KnowledgeBaseManager::exportEntry(int id, const QString& outputPath) {
    KnowledgeEntry e = getEntry(id);
    if (e.id < 0) return false;
    QFile f(outputPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    out << e.markdownContent;
    f.close();
    return true;
}

// ============================================================
// 日期范围搜索
// ============================================================
QList<KnowledgeEntry> KnowledgeBaseManager::getEntriesByDate(const QString& from, const QString& to) {
    QList<KnowledgeEntry> list;
    if (!initialized_) return list;

    QSqlQuery q(db_);
    if (!from.isEmpty() && !to.isEmpty()) {
        q.prepare("SELECT id, title, file_type, source_path, md_path, lang, file_size, summary, created_at "
                  "FROM documents WHERE created_at BETWEEN :from AND :to ORDER BY id DESC");
        q.bindValue(":from", from);
        q.bindValue(":to", to);
    } else if (!from.isEmpty()) {
        q.prepare("SELECT id, title, file_type, source_path, md_path, lang, file_size, summary, created_at "
                  "FROM documents WHERE created_at >= :from ORDER BY id DESC");
        q.bindValue(":from", from);
    } else if (!to.isEmpty()) {
        q.prepare("SELECT id, title, file_type, source_path, md_path, lang, file_size, summary, created_at "
                  "FROM documents WHERE created_at <= :to ORDER BY id DESC");
        q.bindValue(":to", to);
    } else {
        return getAllEntries();
    }
    if (!q.exec()) return list;
    while (q.next()) {
        KnowledgeEntry e;
        e.id             = q.value(0).toInt();
        e.title          = q.value(1).toString();
        e.fileType       = q.value(2).toString();
        e.sourcePath     = q.value(3).toString();
        e.mdFilePath     = q.value(4).toString();
        e.translatedLang = q.value(5).toString();
        e.fileSize       = q.value(6).toLongLong();
        e.summary        = q.value(7).toString();
        e.createdAt      = QDateTime::fromString(q.value(8).toString(), "yyyy-MM-dd hh:mm:ss");
        list.append(e);
    }
    return list;
}

// ============================================================
// 标签 CRUD
// ============================================================
bool KnowledgeBaseManager::addTag(const QString& name) {
    if (!ensureDb()) return false;
    QSqlQuery q(db_);
    q.prepare("INSERT OR IGNORE INTO tags (name) VALUES (:n)");
    q.bindValue(":n", name);
    return q.exec();
}

bool KnowledgeBaseManager::deleteTag(int tagId) {
    if (!ensureDb()) return false;
    QSqlQuery q(db_);
    q.prepare("DELETE FROM tags WHERE id = :id");
    q.bindValue(":id", tagId);
    return q.exec();
}

QList<QPair<int,QString>> KnowledgeBaseManager::getAllTags() {
    QList<QPair<int,QString>> list;
    if (!initialized_) return list;
    QSqlQuery q(db_);
    q.exec("SELECT id, name FROM tags ORDER BY name");
    while (q.next())
        list.append({q.value(0).toInt(), q.value(1).toString()});
    return list;
}

// ============================================================
// 文档-标签关联
// ============================================================
bool KnowledgeBaseManager::setDocumentTags(int docId, const QList<int>& tagIds) {
    if (!ensureDb()) return false;
    db_.transaction();
    QSqlQuery del(db_);
    del.prepare("DELETE FROM document_tags WHERE doc_id = :d");
    del.bindValue(":d", docId);
    if (!del.exec()) { db_.rollback(); return false; }
    QSqlQuery ins(db_);
    ins.prepare("INSERT INTO document_tags (doc_id, tag_id) VALUES (:d, :t)");
    for (int tid : tagIds) {
        ins.bindValue(":d", docId);
        ins.bindValue(":t", tid);
        if (!ins.exec()) { db_.rollback(); return false; }
    }
    return db_.commit();
}

QList<int> KnowledgeBaseManager::getDocumentTagIds(int docId) {
    QList<int> ids;
    if (!initialized_) return ids;
    QSqlQuery q(db_);
    q.prepare("SELECT tag_id FROM document_tags WHERE doc_id = :d");
    q.bindValue(":d", docId);
    if (q.exec())
        while (q.next()) ids.append(q.value(0).toInt());
    return ids;
}

QStringList KnowledgeBaseManager::getDocumentTagNames(int docId) {
    QStringList names;
    if (!initialized_) return names;
    QSqlQuery q(db_);
    q.prepare("SELECT t.name FROM tags t JOIN document_tags dt ON t.id=dt.tag_id WHERE dt.doc_id=:d");
    q.bindValue(":d", docId);
    if (q.exec())
        while (q.next()) names.append(q.value(0).toString());
    return names;
}

// ============================================================
// 搜索
// ============================================================
QList<KnowledgeEntry> KnowledgeBaseManager::searchEntries(const QString& keyword) {
    QList<KnowledgeEntry> list;
    if (!initialized_ || keyword.trimmed().isEmpty()) return getAllEntries();
    QSqlQuery q(db_);
    q.prepare("SELECT id, title, file_type, source_path, md_path, lang, file_size, summary, created_at "
              "FROM documents WHERE title LIKE :kw OR summary LIKE :kw2 ORDER BY id DESC");
    QString like = "%" + keyword.trimmed() + "%";
    q.bindValue(":kw", like);
    q.bindValue(":kw2", like);
    if (!q.exec()) return list;
    while (q.next()) {
        KnowledgeEntry e;
        e.id             = q.value(0).toInt();
        e.title          = q.value(1).toString();
        e.fileType       = q.value(2).toString();
        e.sourcePath     = q.value(3).toString();
        e.mdFilePath     = q.value(4).toString();
        e.translatedLang = q.value(5).toString();
        e.fileSize       = q.value(6).toLongLong();
        e.summary        = q.value(7).toString();
        e.createdAt      = QDateTime::fromString(q.value(8).toString(), "yyyy-MM-dd hh:mm:ss");
        list.append(e);
    }
    return list;
}

QList<KnowledgeEntry> KnowledgeBaseManager::getEntriesByTag(int tagId) {
    QList<KnowledgeEntry> list;
    if (!initialized_) return list;
    QSqlQuery q(db_);
    q.prepare("SELECT d.id, d.title, d.file_type, d.source_path, d.md_path, d.lang, "
              "d.file_size, d.summary, d.created_at "
              "FROM documents d JOIN document_tags dt ON d.id=dt.doc_id "
              "WHERE dt.tag_id=:tid ORDER BY d.id DESC");
    q.bindValue(":tid", tagId);
    if (!q.exec()) return list;
    while (q.next()) {
        KnowledgeEntry e;
        e.id             = q.value(0).toInt();
        e.title          = q.value(1).toString();
        e.fileType       = q.value(2).toString();
        e.sourcePath     = q.value(3).toString();
        e.mdFilePath     = q.value(4).toString();
        e.translatedLang = q.value(5).toString();
        e.fileSize       = q.value(6).toLongLong();
        e.summary        = q.value(7).toString();
        e.createdAt      = QDateTime::fromString(q.value(8).toString(), "yyyy-MM-dd hh:mm:ss");
        list.append(e);
    }
    return list;
}

// ============================================================
// 摘要 & 状态
// ============================================================
bool KnowledgeBaseManager::updateSummary(int docId, const QString& summary) {
    if (!ensureDb()) return false;
    QSqlQuery q(db_);
    q.prepare("UPDATE documents SET summary=:s WHERE id=:id");
    q.bindValue(":s", summary);
    q.bindValue(":id", docId);
    return q.exec();
}

bool KnowledgeBaseManager::updateParseStatus(int docId, const QString& status) {
    if (!ensureDb()) return false;
    QSqlQuery q(db_);
    q.prepare("UPDATE documents SET parse_status=:s WHERE id=:id");
    q.bindValue(":s", status);
    q.bindValue(":id", docId);
    return q.exec();
}
