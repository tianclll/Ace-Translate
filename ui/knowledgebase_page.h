#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QThread>
#include <QList>
#include <QSet>
#include <QEvent>

/// 导入结果结构体（从工作线程发回主线程）
struct ImportResult {
    int index;
    QString filePath;
    QString title;
    QString fileType;
    qint64 fileSize;
    QString markdownContent;  // 引擎解析结果
    bool parseOk = false;
};

struct ImportTask {
    QString filePath;
    QString title;
    QString fileType;
    qint64 fileSize;
};

class DropZoneWidget;

/// 知识库页面：上传区 + 工具栏 + 列表（带多选） + 底部批量操作栏
class KnowledgeBasePage : public QWidget {
    Q_OBJECT
public:
    explicit KnowledgeBasePage(QWidget* parent = nullptr);
    ~KnowledgeBasePage() override;

    void refreshList();
    void refreshTags();

signals:
    void statusMessage(const QString& msg);
    /// 单个文件解析完成（工作线程发出，主线程处理入库）
    void fileParsed(ImportResult result);
    void allImportFinished();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onFileDropped(const QStringList& paths);
    void onBatchImport();
    void onAddNewTag();
    void onSearchTextChanged(const QString& text);
    void onTagFilterChanged(int index);
    void onBatchDelete();
    void onBatchChangeTags();

private:
    void setupUI();
    QString generateSummary(const QString& markdown);
    /// 逐个处理下一个文件（由 QTimer 驱动）
    void processNextFile();
    QWidget* createListItem(int id, const QString& title, const QString& date,
                            const QString& fileType, const QStringList& tags,
                            const QString& summary);
    void updateBatchBar();

    // ---- UI 控件 ----
    DropZoneWidget* dropZone_ = nullptr;
    QLineEdit* searchInput_ = nullptr;
    QComboBox* tagFilterCombo_ = nullptr;

    // 文档列表
    QWidget* listContainer_ = nullptr;
    QVBoxLayout* listLayout_ = nullptr;
    QScrollArea* listScroll_ = nullptr;
    QLabel* emptyHint_ = nullptr;

    // 底部批量操作栏
    QWidget* batchBar_ = nullptr;
    QLabel* batchCountLabel_ = nullptr;
    QPushButton* batchTagBtn_ = nullptr;
    QPushButton* batchDelBtn_ = nullptr;

    // 选中状态
    QSet<int> checkedDocIds_;
    bool inBatchMode_ = false;

    // 导入计数
    int importCount_ = 0;
    int processIndex_ = 0;
    QList<ImportTask> pendingTasks_;
    QString pendingBaseDir_;
};
