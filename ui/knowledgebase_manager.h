#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QList>
#include <QDateTime>

struct KnowledgeEntry {
    int id = -1;
    QString title;           // 原始文件名
    QString fileType;        // 扩展名 pdf/docx/png 等
    QString sourcePath;      // 原始文件路径
    QString mdFilePath;      // 相对路径 knowledge_base/md/<id>.md
    QString translatedLang;  // 目标语言
    qint64 fileSize = 0;     // 原始文件大小
    QDateTime createdAt;
    QString markdownContent; // 仅在 getEntry 时填充的内存缓存
    QString summary;         // AI 生成摘要
    QString parseStatus;     // pending/processing/done/error
};

class KnowledgeBaseManager : public QObject {
    Q_OBJECT
public:
    static KnowledgeBaseManager& getInstance();

    // 初始化数据库（懒加载，首次访问时调用）
    bool initialize(const QString& dbPath = QString());

    // CRUD
    bool addEntry(const KnowledgeEntry& entry, int* outId = nullptr);
    bool deleteEntry(int id);
    int deleteEntries(const QList<int>& ids);  // 批量删除，返回成功删除数
    QList<KnowledgeEntry> getAllEntries(int limit = 100, int offset = 0);
    KnowledgeEntry getEntry(int id);
    int entryCount() const;

    // ---- 标签 CRUD ----
    bool addTag(const QString& name);
    bool deleteTag(int tagId);
    QList<QPair<int,QString>> getAllTags();

    // ---- 文档-标签关联 ----
    bool setDocumentTags(int docId, const QList<int>& tagIds);
    QList<int> getDocumentTagIds(int docId);
    QStringList getDocumentTagNames(int docId);

    // ---- 搜索 ----
    QList<KnowledgeEntry> searchEntries(const QString& keyword);
    QList<KnowledgeEntry> getEntriesByTag(int tagId);

    // ---- 摘要 & 状态 ----
    bool updateSummary(int docId, const QString& summary);
    bool updateParseStatus(int docId, const QString& status);

    // 导出 .md 文件到指定路径
    bool exportEntry(int id, const QString& outputPath);

private:
    KnowledgeBaseManager(QObject* parent = nullptr);
    ~KnowledgeBaseManager() override;
    KnowledgeBaseManager(const KnowledgeBaseManager&) = delete;
    KnowledgeBaseManager& operator=(const KnowledgeBaseManager&) = delete;

    bool createTables();
    QString storagePath() const;
    bool ensureDb();  // 确保数据库连接打开，每次操作前调用

    QSqlDatabase db_;
    QString dbPath_;
    bool initialized_ = false;
};
