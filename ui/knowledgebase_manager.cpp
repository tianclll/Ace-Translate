#include "knowledgebase_manager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QTextStream>

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
    return QCoreApplication::applicationDirPath() + "/knowledge_base/";
}

bool KnowledgeBaseManager::initialize(const QString& dbPath) {
    if (initialized_) return true;

    QString path = dbPath.isEmpty() ? (storagePath() + "knowledge.db") : dbPath;
    QDir().mkpath(QFileInfo(path).absolutePath());

    db_ = QSqlDatabase::addDatabase("QSQLITE", "knowledge_connection");
    db_.setDatabaseName(path);
    if (!db_.open()) {
        qWarning() << "[KB] Failed to open DB:" << db_.lastError().text();
        return false;
    }

    dbPath_ = path;
    initialized_ = createTables();
    return initialized_;
}

bool KnowledgeBaseManager::createTables() {
    QSqlQuery query(db_);
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
        qWarning() << "[KB] Create table failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool KnowledgeBaseManager::addEntry(const KnowledgeEntry& entry, int* outId) {
    if (!initialized_) return false;

    // 先插入占位行获取 ID
    QSqlQuery query(db_);
    query.exec("INSERT INTO documents (title, file_type, source_path, md_path, lang, file_size) "
               "VALUES ('', '', '', '', '', 0)");
    int newId = query.lastInsertId().toInt();

    // 写入 .md 文件
    QString mdDir = storagePath() + "md/";
    QDir().mkpath(mdDir);
    QString mdFilePath = mdDir + QString::number(newId) + ".md";
    QFile file(mdFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
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
    update.bindValue(":mp",  QString("md/%1.md").arg(newId));
    update.bindValue(":l",   entry.translatedLang);
    update.bindValue(":fs",  entry.fileSize);
    update.bindValue(":id",  newId);

    if (!update.exec()) {
        qWarning() << "[KB] Update entry failed:" << update.lastError().text();
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
    if (!initialized_) return false;

    // 删除 .md 文件
    QString mdFile = storagePath() + "md/" + QString::number(id) + ".md";
    QFile::remove(mdFile);

    // 删除 DB 行
    QSqlQuery query(db_);
    query.prepare("DELETE FROM documents WHERE id = :id");
    query.bindValue(":id", id);
    return query.exec();
}

QList<KnowledgeEntry> KnowledgeBaseManager::getAllEntries(int limit, int offset) {
    QList<KnowledgeEntry> list;
    if (!initialized_) return list;

    QSqlQuery query(db_);
    query.prepare("SELECT id, title, file_type, source_path, md_path, lang, file_size, created_at "
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
        e.createdAt      = QDateTime::fromString(query.value(7).toString(), "yyyy-MM-dd hh:mm:ss");
        list.append(e);
    }
    return list;
}

KnowledgeEntry KnowledgeBaseManager::getEntry(int id) {
    KnowledgeEntry e;
    if (!initialized_) return e;

    QSqlQuery query(db_);
    query.prepare("SELECT id, title, file_type, source_path, md_path, lang, file_size, created_at "
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
    e.createdAt      = QDateTime::fromString(query.value(7).toString(), "yyyy-MM-dd hh:mm:ss");

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
