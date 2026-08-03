#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QTextEdit>
#include <QStringList>
#include <QCloseEvent>

class KnowledgeBaseManager;  // forward decl

/**
 * @brief 专有词管理对话框
 *
 * 样式沿用项目 RoundedDlg 风格：
 * - FramelessWindowHint + WA_TranslucentBackground
 * - 白色容器 #FFFFFF，border-radius: 14px
 * - 主按钮 #primaryBtn（#0B7C72 背景），次按钮 #secondaryBtn
 */
class GlossaryDialog : public QDialog {
    Q_OBJECT
public:
    explicit GlossaryDialog(QWidget* parent = nullptr);
    ~GlossaryDialog() override = default;

private slots:
    void onAddTerm();
    void onSaveEdit();
    void onDeleteTerm();
    void onClearAll();
    void onImportFile();
    void onImportPastedText();
    void onSelectionChanged();
    void onSourceLangChanged(int index);
    void onTargetLangChanged(int index);

private:
    void setupUI();
    void refreshList();
    void loadTerms();
    void selectTerm(int termId, const QString& term, const QString& translation);
    void applyDialogSize();
    void closeEvent(QCloseEvent* event) override;

    // ---- 语言列表 ----
    QStringList supportedLangs() const;
    static QStringList allLanguages();

    // ---- UI 控件 ----
    QComboBox* sourceLangCombo_ = nullptr;
    QComboBox* targetLangCombo_ = nullptr;
    QListWidget* termList_ = nullptr;
    QLineEdit* termEdit_ = nullptr;
    QLineEdit* translationEdit_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QPushButton* importBtn_ = nullptr;
    QPushButton* importFromTextBtn_ = nullptr;  // 粘贴导入按钮
    QPushButton* clearAllBtn_ = nullptr;
    QPushButton* closeBtn_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QTextEdit* importTextEdit_ = nullptr;  // 粘贴导入文本区
    QWidget* importPanel_ = nullptr;  // 导入面板（默认隐藏）

    // ---- 编辑状态 ----
    int editingTermId_ = -1;   // -1 表示新建模式，>=0 表示编辑现有术语
    bool isNewTerm_ = true;
};
