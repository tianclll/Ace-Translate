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
#include <QList>
#include <QSet>
#include <QEvent>

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
};
