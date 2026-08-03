#include "glossary_dialog.h"
#include "knowledgebase_manager.h"
#include "toast.h"
#include "docmind/core/GlossaryInjector.hpp"
#include <QApplication>
#include <QCloseEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScreen>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTextStream>
#include <QTextEdit>

// ============================================================
// GlossaryDialog
// ============================================================

GlossaryDialog::GlossaryDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setupUI();
    loadTerms();
    docmind::GlossaryInjector::refreshFromDB();
    applyDialogSize();
}

QStringList GlossaryDialog::allLanguages() {
    QStringList langs;
    langs << QApplication::translate("MainWindow", "Chinese")
          << QApplication::translate("MainWindow", "English")
          << QApplication::translate("MainWindow", "Japanese")
          << QApplication::translate("MainWindow", "Korean")
          << QApplication::translate("MainWindow", "French")
          << QApplication::translate("MainWindow", "German")
          << QApplication::translate("MainWindow", "Spanish")
          << QApplication::translate("MainWindow", "Portuguese")
          << QApplication::translate("MainWindow", "Russian")
          << QApplication::translate("MainWindow", "Arabic")
          << QApplication::translate("MainWindow", "Thai")
          << QApplication::translate("MainWindow", "Vietnamese")
          << QApplication::translate("MainWindow", "Italian")
          << QApplication::translate("MainWindow", "Dutch")
          << QApplication::translate("MainWindow", "Polish")
          << QApplication::translate("MainWindow", "Turkish")
          << QApplication::translate("MainWindow", "Hindi")
          << QApplication::translate("MainWindow", "Indonesian")
          << QApplication::translate("MainWindow", "Malay")
          << QApplication::translate("MainWindow", "Swedish")
          << QApplication::translate("MainWindow", "Czech")
          << QApplication::translate("MainWindow", "Greek")
          << QApplication::translate("MainWindow", "Romanian")
          << QApplication::translate("MainWindow", "Danish")
          << QApplication::translate("MainWindow", "Finnish")
          << QApplication::translate("MainWindow", "Norwegian")
          << QApplication::translate("MainWindow", "Hungarian")
          << QApplication::translate("MainWindow", "Ukrainian")
          << QApplication::translate("MainWindow", "Bengali")
          << QApplication::translate("MainWindow", "Filipino")
          << QApplication::translate("MainWindow", "Burmese")
          << QApplication::translate("MainWindow", "Khmer")
          << QApplication::translate("MainWindow", "Lao")
          << QApplication::translate("MainWindow", "Nepali")
          << QApplication::translate("MainWindow", "Persian")
          << QApplication::translate("MainWindow", "Swahili");
    return langs;
}


QStringList GlossaryDialog::supportedLangs() const {
    return allLanguages();
}


void GlossaryDialog::setupUI() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);

    auto* container = new QWidget;
    container->setObjectName("kbRoundedDlg");
    container->setStyleSheet(
        "QWidget#kbRoundedDlg { background: #FFFFFF; border-radius: 14px; }");
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(20, 18, 20, 16);
    lay->setSpacing(10);

    // ---- 标题栏 ----
    auto* titleBar = new QHBoxLayout;
    titleBar->setContentsMargins(0, 0, 0, 0);
    titleBar->setSpacing(0);

    auto* title = new QLabel(tr("Terminology Management"));
    title->setStyleSheet("font-size: 15px; font-weight: 600; color: #1C1C1E;");
    titleBar->addWidget(title);
    titleBar->addStretch();

    closeBtn_ = new QPushButton;
    closeBtn_->setFixedSize(28, 28);
    closeBtn_->setCursor(Qt::PointingHandCursor);
    closeBtn_->setFlat(true);
    closeBtn_->setIcon(QIcon(":/icons/close.png"));
    closeBtn_->setIconSize(QSize(16, 16));
    closeBtn_->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 6px;"
        " }"
        "QPushButton:hover { background: #F3F4F6; }");
    connect(closeBtn_, &QPushButton::clicked, this, &QDialog::reject);
    titleBar->addWidget(closeBtn_);

    lay->addLayout(titleBar);

    // ---- 语言对 + 按钮行 ----
    auto* langRow = new QHBoxLayout;
    langRow->setSpacing(8);

    sourceLangCombo_ = new QComboBox;
    sourceLangCombo_->addItems(supportedLangs());
    sourceLangCombo_->setFixedHeight(30);
    sourceLangCombo_->setMinimumWidth(120);
    sourceLangCombo_->setStyleSheet(
        "QComboBox { border: 1px solid #DDE1E5; border-radius: 6px; padding: 0 10px;"
        " font-size: 12px; background: #FFFFFF; color: #1C1C1E; }"
        "QComboBox:focus { border-color: #0B7C72; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { border: 1px solid #E8ECEF; border-radius: 6px;"
        " selection-background-color: #E8F0EF; }");
    connect(sourceLangCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GlossaryDialog::onSourceLangChanged);

    auto* arrow1 = new QLabel("→");
    arrow1->setStyleSheet("font-size: 14px; color: #9CA3AF; background: transparent;");
    arrow1->setFixedWidth(16);

    targetLangCombo_ = new QComboBox;
    targetLangCombo_->addItems(supportedLangs());
    targetLangCombo_->setFixedHeight(30);
    targetLangCombo_->setMinimumWidth(120);
    targetLangCombo_->setCurrentText(QApplication::translate("MainWindow", "Chinese"));
    targetLangCombo_->setStyleSheet(sourceLangCombo_->styleSheet());
    connect(targetLangCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GlossaryDialog::onTargetLangChanged);

    langRow->addWidget(sourceLangCombo_);
    langRow->addWidget(arrow1);
    langRow->addWidget(targetLangCombo_);
    langRow->addSpacing(12);

    addBtn_ = new QPushButton(tr("Add"));
    addBtn_->setObjectName("primaryBtn");
    addBtn_->setFixedHeight(30);
    addBtn_->setCursor(Qt::PointingHandCursor);
    connect(addBtn_, &QPushButton::clicked, this, &GlossaryDialog::onAddTerm);
    langRow->addWidget(addBtn_);

    // 导入按钮：切换粘贴导入面板
    importBtn_ = new QPushButton(tr("Import"));
    importBtn_->setObjectName("secondaryBtn");
    importBtn_->setFixedHeight(30);
    importBtn_->setCursor(Qt::PointingHandCursor);
    connect(importBtn_, &QPushButton::clicked, this, [this]() {
        if (importPanel_->isVisible()) {
            importPanel_->hide();
            importBtn_->setText(tr("Import"));
        } else {
            importPanel_->show();
            importTextEdit_->clear();
            importTextEdit_->setFocus();
            importBtn_->setText(tr("Cancel"));
        }
    });
    langRow->addWidget(importBtn_);

    langRow->addStretch();
    lay->addLayout(langRow);

    // ---- 主体：列表 + 编辑面板 ----
    auto* bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(12);

    auto* listWrap = new QVBoxLayout;
    listWrap->setContentsMargins(0, 0, 0, 0);
    listWrap->setSpacing(0);

    termList_ = new QListWidget;
    termList_->setStyleSheet(
        "QListWidget { border: 1px solid #E8ECEF; border-radius: 8px;"
        " background: #FAFBFC; font-size: 12px; outline: none; }"
        "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid #F0F0F0; }"
        "QListWidget::item:selected { background: #E8F0EF; color: #0B7C72; }"
        "QListWidget::item:hover { background: #F5F7F7; }");
    termList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(termList_, &QListWidget::itemSelectionChanged,
            this, &GlossaryDialog::onSelectionChanged);
    listWrap->addWidget(termList_);

    bodyRow->addLayout(listWrap, 1);

    // 编辑面板
    auto* editPanel = new QFrame;
    editPanel->setStyleSheet("QFrame { background: #F8FAFA; border-radius: 8px; border: 1px solid #F0F0F0; }");
    auto* editLayout = new QVBoxLayout(editPanel);
    editLayout->setContentsMargins(14, 12, 14, 12);
    editLayout->setSpacing(10);

    auto* termLabel = new QLabel(tr("Term"));
    termLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #6B7280; background: transparent; border: none;");
    editLayout->addWidget(termLabel);

    termEdit_ = new QLineEdit;
    termEdit_->setPlaceholderText(tr("Term"));
    termEdit_->setFixedHeight(32);
    termEdit_->setStyleSheet(
        "QLineEdit { border: 1px solid #DDE1E5; border-radius: 6px; padding: 0 10px;"
        " font-size: 13px; background: #FFFFFF; color: #1A1A2E; }"
        "QLineEdit:focus { border-color: #0B7C72; }");
    editLayout->addWidget(termEdit_);

    auto* transLabel = new QLabel(tr("Translation"));
    transLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #6B7280; background: transparent; border: none;");
    editLayout->addWidget(transLabel);

    translationEdit_ = new QLineEdit;
    translationEdit_->setPlaceholderText(tr("Translation"));
    translationEdit_->setFixedHeight(32);
    translationEdit_->setStyleSheet(termEdit_->styleSheet());
    editLayout->addWidget(translationEdit_);

    editLayout->addStretch();

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    saveBtn_ = new QPushButton(tr("Save"));
    saveBtn_->setObjectName("primaryBtn");
    saveBtn_->setFixedHeight(32);
    saveBtn_->setCursor(Qt::PointingHandCursor);
    connect(saveBtn_, &QPushButton::clicked, this, &GlossaryDialog::onSaveEdit);
    btnRow->addWidget(saveBtn_);

    deleteBtn_ = new QPushButton(tr("Delete"));
    deleteBtn_->setObjectName("secondaryBtn");
    deleteBtn_->setFixedHeight(32);
    deleteBtn_->setCursor(Qt::PointingHandCursor);
    connect(deleteBtn_, &QPushButton::clicked, this, &GlossaryDialog::onDeleteTerm);
    btnRow->addWidget(deleteBtn_);

    editLayout->addLayout(btnRow);
    bodyRow->addWidget(editPanel, 1);

    lay->addLayout(bodyRow, 1);

    // ---- 导入面板（默认隐藏）----
    importPanel_ = new QWidget;
    importPanel_->setVisible(false);
    auto* importLayout = new QVBoxLayout(importPanel_);
    importLayout->setContentsMargins(0, 8, 0, 0);
    importLayout->setSpacing(6);

    auto* importHint = new QLabel(tr("Paste terminology text below, one term per line:"));
    importHint->setStyleSheet("font-size: 11px; color: #6B7280; background: transparent; border: none;");
    importLayout->addWidget(importHint);

    importTextEdit_ = new QTextEdit;
    importTextEdit_->setPlaceholderText(
        tr("term translation\n"
           "transformer 变换器\n"
           "self-attention 自注意力机制\n"
           "BERT BERT"));
    importTextEdit_->setFixedHeight(80);
    importTextEdit_->setStyleSheet(
        "QTextEdit { border: 1px solid #DDE1E5; border-radius: 6px; padding: 8px;"
        " font-size: 12px; background: #FFFFFF; color: #1A1A2E; }"
        "QTextEdit:focus { border-color: #0B7C72; }");
    importLayout->addWidget(importTextEdit_);

    auto* importBtnRow = new QHBoxLayout;
    importBtnRow->setSpacing(8);
    importBtnRow->addStretch();

    importFromTextBtn_ = new QPushButton(tr("Import"));
    importFromTextBtn_->setObjectName("primaryBtn");
    importFromTextBtn_->setFixedHeight(28);
    importFromTextBtn_->setCursor(Qt::PointingHandCursor);
    connect(importFromTextBtn_, &QPushButton::clicked, this, &GlossaryDialog::onImportPastedText);
    importBtnRow->addWidget(importFromTextBtn_);

    auto* importCancelBtn = new QPushButton(tr("Cancel"));
    importCancelBtn->setFixedHeight(28);
    importCancelBtn->setCursor(Qt::PointingHandCursor);
    importCancelBtn->setStyleSheet(
        "QPushButton { border: 1px solid #DDE1E5; border-radius: 6px; padding: 0 12px;"
        " background: transparent; color: #374151; font-size: 12px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(importCancelBtn, &QPushButton::clicked, this, [this]() {
        importPanel_->hide();
        importBtn_->setText(tr("Import"));
    });
    importBtnRow->addWidget(importCancelBtn);

    importLayout->addLayout(importBtnRow);
    lay->addWidget(importPanel_);

    // ---- 底部栏 ----
    auto* bottomRow = new QHBoxLayout;
    bottomRow->setContentsMargins(0, 0, 0, 0);

    countLabel_ = new QLabel;
    countLabel_->setStyleSheet("font-size: 11px; color: #889096; background: transparent; border: none;");
    bottomRow->addWidget(countLabel_);
    bottomRow->addStretch();

    clearAllBtn_ = new QPushButton(tr("Clear All"));
    clearAllBtn_->setCursor(Qt::PointingHandCursor);
    clearAllBtn_->setStyleSheet(
        "QPushButton { border: none; border-radius: 4px; padding: 4px 10px;"
        " background: transparent; color: #DC2626; font-size: 11px; }"
        "QPushButton:hover { background: #FEF2F2; }");
    connect(clearAllBtn_, &QPushButton::clicked, this, &GlossaryDialog::onClearAll);
    bottomRow->addWidget(clearAllBtn_);

    lay->addLayout(bottomRow);

    outer->addWidget(container);
}

void GlossaryDialog::applyDialogSize() {
    // Walk up to the top-level window (same pattern as knowledgebase_page RoundedDlg)
    QWidget* win = this;
    while (win->parentWidget()) win = win->parentWidget();

    // Size from content hint, clamped to window
    QSize sh = layout()->sizeHint();
    int w = qMax(sh.width(), 720);
    int h = qMin(sh.height(), qMax(440, win->height() - 120));
    int maxW = win->width() - 48;
    resize(qMin(w, maxW), h);

    // Center in the top-level window
    QRect g = win->geometry();
    move(g.x() + g.width() / 2 - width() / 2,
         g.y() + g.height() / 2 - height() / 2);
}

void GlossaryDialog::loadTerms() {
    auto& km = KnowledgeBaseManager::getInstance();
    QString srcLang = sourceLangCombo_->currentText();
    QString tgtLang = targetLangCombo_->currentText();

    auto terms = km.getGlossaryForLang(srcLang, tgtLang);
    termList_->clear();

    for (const auto& [term, trans] : terms) {
        auto* item = new QListWidgetItem;
        QSqlDatabase db = QSqlDatabase::database("kb_conn");
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("SELECT id FROM glossary_terms WHERE term=:t AND translation=:tr "
                      "AND source_lang=:s AND target_lang=:tg LIMIT 1");
            q.bindValue(":t", term);
            q.bindValue(":tr", trans);
            q.bindValue(":s", srcLang);
            q.bindValue(":tg", tgtLang);
            if (q.exec() && q.next()) {
                item->setData(Qt::UserRole + 2, q.value(0).toInt());
            }
        }
        item->setData(Qt::UserRole, term);
        item->setData(Qt::UserRole + 1, trans);
        QString display = QString("%1  →  %2").arg(term, trans);
        item->setText(display);
        termList_->addItem(item);
    }

    countLabel_->setText(tr("Total %1 terms").arg(termList_->count()));

    editingTermId_ = -1;
    isNewTerm_ = true;
    termEdit_->clear();
    translationEdit_->clear();
    termEdit_->setEnabled(true);
}

void GlossaryDialog::onAddTerm() {
    editingTermId_ = -1;
    isNewTerm_ = true;
    termEdit_->clear();
    translationEdit_->clear();
    termEdit_->setFocus();
    termList_->clearSelection();
}

void GlossaryDialog::onSaveEdit() {
    QString term = termEdit_->text().trimmed();
    QString trans = translationEdit_->text().trimmed();

    if (term.isEmpty() || trans.isEmpty()) {
        ToastNotification::show(this, tr("Term and translation cannot be empty"), 3000);
        return;
    }

    auto& km = KnowledgeBaseManager::getInstance();
    QString srcLang = sourceLangCombo_->currentText();
    QString tgtLang = targetLangCombo_->currentText();

    if (isNewTerm_) {
        if (!km.addGlossaryTerm(term, trans, srcLang, tgtLang)) {
            ToastNotification::show(this, tr("Failed to add term"), 3000);
            return;
        }
    } else {
        if (editingTermId_ > 0) {
            km.deleteGlossaryTerm(editingTermId_);
        }
        if (!km.addGlossaryTerm(term, trans, srcLang, tgtLang)) {
            ToastNotification::show(this, tr("Failed to update term"), 3000);
            return;
        }
    }

    docmind::GlossaryInjector::refreshFromDB();
    loadTerms();
    ToastNotification::show(this, isNewTerm_ ? tr("Term added") : tr("Term updated"), 2000);
}

void GlossaryDialog::onDeleteTerm() {
    auto* item = termList_->currentItem();
    if (!item) return;

    int termId = item->data(Qt::UserRole + 2).toInt();
    if (termId <= 0) {
        termEdit_->clear();
        translationEdit_->clear();
        editingTermId_ = -1;
        isNewTerm_ = true;
        termList_->clearSelection();
        return;
    }

    auto& km = KnowledgeBaseManager::getInstance();
    if (!km.deleteGlossaryTerm(termId)) {
        ToastNotification::show(this, tr("Failed to delete term"), 3000);
        return;
    }

    loadTerms();
    docmind::GlossaryInjector::refreshFromDB();
    ToastNotification::show(this, tr("Term deleted"), 2000);
}

void GlossaryDialog::onClearAll() {
    if (termList_->count() == 0) return;

    for (int i = 0; i < termList_->count(); ++i) {
        auto* item = termList_->item(i);
        int termId = item->data(Qt::UserRole + 2).toInt();
        if (termId > 0) {
            KnowledgeBaseManager::getInstance().deleteGlossaryTerm(termId);
        }
    }
    loadTerms();
    docmind::GlossaryInjector::refreshFromDB();
    ToastNotification::show(this, tr("All terms cleared"), 2000);
}

void GlossaryDialog::onImportFile() {
    // 切换导入面板（供 MOC 信号连接使用，实际逻辑在 lambda 中）
    if (importPanel_->isVisible()) {
        importPanel_->hide();
        importBtn_->setText(tr("Import"));
    } else {
        importPanel_->show();
        importTextEdit_->clear();
        importTextEdit_->setFocus();
        importBtn_->setText(tr("Cancel"));
    }
}

void GlossaryDialog::onImportPastedText() {
    QString rawText = importTextEdit_->toPlainText().trimmed();
    if (rawText.isEmpty()) return;

    auto& km = KnowledgeBaseManager::getInstance();
    QString srcLang = sourceLangCombo_->currentText();
    QString tgtLang = targetLangCombo_->currentText();
    int imported = 0;

    QStringList lines = rawText.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        QStringList parts;
        if (trimmed.contains('\t')) {
            parts = trimmed.split('\t');
        } else if (trimmed.contains(',')) {
            parts = trimmed.split(',');
        } else {
            int sp = trimmed.indexOf(' ');
            if (sp > 0) {
                parts << trimmed.left(sp) << trimmed.mid(sp + 1).trimmed();
            }
        }

        if (parts.size() >= 2) {
            QString term = parts[0].trimmed();
            QString trans = parts[1].trimmed();
            if (!term.isEmpty() && !trans.isEmpty()) {
                km.addGlossaryTerm(term, trans, srcLang, tgtLang);
                imported++;
            }
        }
    }

    docmind::GlossaryInjector::refreshFromDB();
    loadTerms();

    importPanel_->hide();
    importBtn_->setText(tr("Import"));
    importTextEdit_->clear();

    ToastNotification::show(this,
        tr("Imported %1 terms").arg(imported), 2000);
}

void GlossaryDialog::onSelectionChanged() {
    auto* item = termList_->currentItem();
    if (!item) return;

    QString term = item->data(Qt::UserRole).toString();
    QString trans = item->data(Qt::UserRole + 1).toString();
    editingTermId_ = item->data(Qt::UserRole + 2).toInt();

    isNewTerm_ = false;
    termEdit_->setText(term);
    translationEdit_->setText(trans);
}

void GlossaryDialog::onSourceLangChanged(int) {
    loadTerms();
}

void GlossaryDialog::onTargetLangChanged(int) {
    loadTerms();
}

void GlossaryDialog::closeEvent(QCloseEvent* event) {
    event->accept();
}
