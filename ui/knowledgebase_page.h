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
#include <QDateTimeEdit>
#include <QThread>
#include <QList>
#include <QSet>
#include <QEvent>
#include <QPointer>

class QTimer;

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

    /// 双击查看文档详情
    void showDocumentDetail(int docId);

signals:
    void statusMessage(const QString& msg);
    /// 导入进行中（true=正在处理，false=结束），用于驱动主窗口底部状态栏沙漏
    void busyChanged(bool busy);
    /// 单个文件解析完成（工作线程发出，主线程处理入库）
    void fileParsed(ImportResult result);
    void allImportFinished();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onFileDropped(const QStringList& paths);
    void onAddNewTag();
    void onTagFilterChanged(int index);
    void onDateFilterChanged();
    void onSelectAll();
    void onBatchDelete();
    void onBatchChangeTags();
    void onBatchExport();
    void onSummaryReady(int docId, const QString& summary);

private:
    void setupUI();
    QString generateSummary(const QString& markdown);
    /// 逐个处理下一个文件（由 QTimer 驱动）
    void processNextFile(int myGen = 0);
    void finishEntry(const ImportTask& task, const QString& markdown);
    QWidget* createListItem(int id, const QString& title, const QString& date,
                            const QString& fileType, const QStringList& tags,
                            const QString& summary, const QString& keyword = QString());
    void updateBatchBar();
    /// 单击行：折叠/展开摘要（随延迟定时器触发，区分双击）
    void doRowSingleClick(int docId);
    std::string extract_image_text(const std::string& image_path);
    std::string extract_pdf_text(const std::string& pdf_path, const std::string& base_dir, int dpi);

    // ---- UI 控件 ----
    DropZoneWidget* dropZone_ = nullptr;
    QLineEdit* searchInput_ = nullptr;
    QDateTimeEdit* dateFrom_ = nullptr;
    QDateTimeEdit* dateTo_ = nullptr;
    QComboBox* tagFilterCombo_ = nullptr;
    QPushButton* selectAllBtn_ = nullptr;
    bool allSelected_ = false;

    // 文档列表
    QWidget* listCard_ = nullptr;
    QWidget* listContainer_ = nullptr;
    QVBoxLayout* listLayout_ = nullptr;
    QScrollArea* listScroll_ = nullptr;
    QFrame* listHeader_ = nullptr;  // 列表表头（放在滚动区内，第 0 项）
    QLabel* emptyHint_ = nullptr;

    // 底部批量操作栏
    QWidget* batchBar_ = nullptr;
    QLabel* batchCountLabel_ = nullptr;
    QPushButton* batchTagBtn_ = nullptr;
    QPushButton* batchExportBtn_ = nullptr;
    QPushButton* batchDelBtn_ = nullptr;

    // 选中状态
    QSet<int> checkedDocIds_;

    // 行单击/双击手势（单次释放延迟，以区分单击展开 / 双击选中）
    QTimer* rowGestureTimer_ = nullptr;
    int rowGestureDocId_ = 0;
    QPointer<QWidget> rowGestureSumRow_;
    QPointer<QWidget> rowGestureSumToggle_;  // 记录待执行「单击展开」的目标行

    // 导入计数
    int importCount_ = 0;
    int processIndex_ = 0;
    bool isImporting_ = false;
    int importGeneration_ = 0;
    QList<ImportTask> pendingTasks_;
    QString pendingBaseDir_;
};
