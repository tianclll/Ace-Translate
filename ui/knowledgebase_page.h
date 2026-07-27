#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QList>
#include <QEvent>

class DropZoneWidget;

/// 知识库页面：双栏布局（左列表 + 右详情面板）
/// 包含上传归档、标签管理、全文搜索、引擎解析、翻译导出等功能
class KnowledgeBasePage : public QWidget {
    Q_OBJECT
public:
    explicit KnowledgeBasePage(QWidget* parent = nullptr);
    ~KnowledgeBasePage() override;

    /// 刷新文档列表（从 DB 重载）
    void refreshList();
    /// 刷新标签下拉框
    void refreshTags();

signals:
    void statusMessage(const QString& msg);

protected:
    /// 处理文档列表项的点击选中
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    // 上传
    void onFileDropped(const QStringList& paths);
    void onBatchImport();
    // 文档操作
    void onDocItemClicked(int id);
    void onDeleteEntry();
    void onExportMD();
    void onTranslateFullText();
    // 标签
    void onAddNewTag();
    void onChangeTags();
    // 搜索/筛选
    void onSearchTextChanged(const QString& text);
    void onTagFilterChanged(int index);

private:
    void setupUI();
    /// 加载指定文档到详情面板
    void loadDocDetail(int id);
    /// 用已有引擎生成摘要（取 md 前 500 字翻译）
    QString generateSummary(const QString& markdown);
    /// 创建单条文档列表项
    QWidget* createListItem(int id, const QString& title, const QString& date,
                            const QString& fileType, const QStringList& tags);

    // ---- UI 控件 ----
    DropZoneWidget* dropZone_ = nullptr;
    QLineEdit* searchInput_ = nullptr;
    QComboBox* tagFilterCombo_ = nullptr;

    // 左侧列表
    QWidget* listContainer_ = nullptr;
    QVBoxLayout* listLayout_ = nullptr;
    QScrollArea* listScroll_ = nullptr;
    QLabel* emptyHint_ = nullptr;

    // 右侧详情
    QWidget* detailPanel_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QPlainTextEdit* mdPreview_ = nullptr;
    QPushButton* translateBtn_ = nullptr;
    QPushButton* exportBtn_ = nullptr;
    QPushButton* tagBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;

    // 状态
    int currentDocId_ = -1;
    /// 缓存选中文档的标签 ID，用于修改标签弹窗
    QList<int> currentTagIds_;
};
