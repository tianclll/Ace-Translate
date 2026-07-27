#include "knowledgebase_page.h"
#include "knowledgebase_manager.h"
#include "mainwindow.h"        // DropZoneWidget

#include <QFrame>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDateTime>
#include <QDialog>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QTimer>

// ============================================================
// 构造函数
// ============================================================
KnowledgeBasePage::KnowledgeBasePage(QWidget* parent)
    : QWidget(parent) {
    setupUI();

    // 延迟初始化数据库（页面显示后）
    QTimer::singleShot(0, this, [this]() {
        KnowledgeBaseManager::getInstance().initialize();
        refreshList();
    });
}

KnowledgeBasePage::~KnowledgeBasePage() = default;

// ============================================================
// setupUI — 参考 createFilePanel 样式
// ============================================================
void KnowledgeBasePage::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // ---- 标题 ----
    auto* title = new QLabel(tr("Knowledge Base"));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(tr("Archive translated documents"));
    hint->setObjectName("sectionHint");
    layout->addWidget(hint);

    // ---- 拖拽区（复用 DropZoneWidget）----
    dropZone_ = new DropZoneWidget;
    dropZone_->setFixedHeight(120);
    dropZone_->setStyleSheet(QStringLiteral(
        "DropZoneWidget { background: #F0F7F6; border: 2px dashed #D0E8E4; border-radius: 12px; }"
        "DropZoneWidget:hover { background: #E8F5F3; border-color: #0B7C72; }"
    ));
    auto* dropLayout = new QVBoxLayout(dropZone_);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(6);

    auto* dropIcon = new QLabel(QStringLiteral("\U0001F4DA"));
    dropIcon->setAlignment(Qt::AlignCenter);
    dropIcon->setStyleSheet("font-size: 32px; background: transparent; border: none;");
    dropLayout->addWidget(dropIcon);

    auto* dropText = new QLabel(tr("Drop files here to archive"));
    dropText->setAlignment(Qt::AlignCenter);
    dropText->setStyleSheet("font-size: 14px; font-weight: 600; color: #374151; background: transparent; border: none;");
    dropLayout->addWidget(dropText);

    auto* dropHint = new QLabel(tr("PDF / MD / TXT / Images"));
    dropHint->setAlignment(Qt::AlignCenter);
    dropHint->setStyleSheet("font-size: 12px; color: #9CA3AF; background: transparent; border: none;");
    dropLayout->addWidget(dropHint);

    connect(dropZone_, &DropZoneWidget::fileDropped, this, &KnowledgeBasePage::onFileDropped);
    layout->addWidget(dropZone_);

    // ---- 文档列表卡片 ----
    auto* listCard = new QFrame;
    listCard->setStyleSheet(QStringLiteral(
        "QFrame { background: #F5F7F7; border: 1px solid #E8ECEF; border-radius: 10px; }"
    ));
    auto* listCardLayout = new QVBoxLayout(listCard);
    listCardLayout->setContentsMargins(10, 8, 10, 8);
    listCardLayout->setSpacing(6);

    auto* listHeader = new QHBoxLayout;
    auto* listTitle = new QLabel(tr("Archived Documents"));
    listTitle->setStyleSheet(
        "font-size: 12px; font-weight: 600; color: #889096; background: transparent;"
        " text-transform: uppercase;");
    listHeader->addWidget(listTitle);
    listHeader->addStretch();

    countLabel_ = new QLabel;
    countLabel_->setStyleSheet("font-size: 11px; color: #9CA3AF; background: transparent;");
    listHeader->addWidget(countLabel_);
    listCardLayout->addLayout(listHeader);

    scrollArea_ = new QScrollArea;
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setMinimumHeight(100);
    scrollArea_->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }");
    listContainer_ = new QWidget;
    listContainer_->setStyleSheet("QWidget { background: transparent; }");
    listLayout_ = new QVBoxLayout(listContainer_);
    listLayout_->setContentsMargins(0, 0, 0, 0);
    listLayout_->setSpacing(4);

    emptyHint_ = new QLabel(tr("No documents yet.\nDrag files here or archive from translation results."));
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setStyleSheet("color: #9CA3AF; font-size: 13px; padding: 40px; background: transparent; border: none;");
    emptyHint_->setWordWrap(true);
    listLayout_->addWidget(emptyHint_);

    listLayout_->addStretch();
    scrollArea_->setWidget(listContainer_);
    listCardLayout->addWidget(scrollArea_, 1);
    layout->addWidget(listCard, 1);
}

// ============================================================
// refreshList — 从数据库加载文档列表
// ============================================================
void KnowledgeBasePage::refreshList() {
    // 清除旧项目（保留第 0 项 emptyHint_ 和第最后一项 stretch）
    while (listLayout_->count() > 2) {
        auto* item = listLayout_->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    auto docs = KnowledgeBaseManager::getInstance().getAllEntries();
    int count = docs.size();

    // 空状态
    emptyHint_->setVisible(count == 0);
    countLabel_->setText(tr("%1 document(s)").arg(count));

    for (const auto& doc : docs) {
        auto* widget = createListItem(doc.id, doc.title, doc.fileType,
                                      doc.createdAt.toString("yyyy-MM-dd hh:mm"),
                                      doc.translatedLang);
        listLayout_->insertWidget(listLayout_->count() - 1, widget); // 在 stretch 前
    }
}

// ============================================================
// createListItem — 单个文档项
// ============================================================
QWidget* KnowledgeBasePage::createListItem(int id, const QString& title,
                                            const QString& type,
                                            const QString& date,
                                            const QString& lang) {
    auto* item = new QFrame;
    item->setStyleSheet(
        "QFrame { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 6px; }"
        "QFrame:hover { background: #F5F7F7; }");
    auto* row = new QHBoxLayout(item);
    row->setContentsMargins(10, 8, 10, 8);
    row->setSpacing(8);

    // 标题
    auto* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-weight: 600; color: #1A1A2E; font-size: 13px; background: transparent; border: none;");
    titleLabel->setMinimumWidth(120);
    row->addWidget(titleLabel);

    // 类型标签
    if (!type.isEmpty()) {
        auto* typeLabel = new QLabel(type.toUpper());
        typeLabel->setStyleSheet(QStringLiteral(
            "background: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 10px;"
            " padding: 0 8px; font-size: 10px; font-weight: 500; color: #6B7280;"));
        typeLabel->setFixedHeight(20);
        row->addWidget(typeLabel);
    }

    // 时间
    auto* dateLabel = new QLabel(date);
    dateLabel->setStyleSheet("font-size: 11px; color: #889096; background: transparent; border: none;");
    row->addWidget(dateLabel);

    // 语言
    if (!lang.isEmpty()) {
        auto* langLabel = new QLabel(lang);
        langLabel->setStyleSheet("font-size: 11px; color: #0B7C72; background: transparent; border: none;");
        row->addWidget(langLabel);
    }

    row->addStretch();

    // 操作按钮
    auto makeBtn = [](const QString& text, int w) -> QPushButton* {
        auto* btn = new QPushButton(text);
        btn->setFixedSize(w, 24);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { border: 1px solid #D1D5DB; border-radius: 4px;"
            " background: transparent; color: #374151; font-size: 11px; }"
            "QPushButton:hover { background: #F0F7F6; border-color: #0B7C72; color: #0B7C72; }");
        return btn;
    };

    auto* viewBtn = makeBtn(tr("View"), 50);
    connect(viewBtn, &QPushButton::clicked, this, [this, id]() { onViewEntry(id); });
    row->addWidget(viewBtn);

    auto* exportBtn = makeBtn(tr("Export"), 60);
    connect(exportBtn, &QPushButton::clicked, this, [this, id]() { onExportEntry(id); });
    row->addWidget(exportBtn);

    auto* delBtn = new QPushButton(QStringLiteral("✕"));
    delBtn->setFixedSize(24, 24);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet(
        "QPushButton { border: none; color: #9CA3AF; font-size: 14px; }"
        "QPushButton:hover { color: #EF4444; }");
    connect(delBtn, &QPushButton::clicked, this, [this, id]() { onDeleteEntry(id); });
    row->addWidget(delBtn);

    return item;
}

// ============================================================
// 文件拖入/点击上传
// ============================================================
void KnowledgeBasePage::onFileDropped(const QStringList& paths) {
    QStringList files = paths;
    if (files.isEmpty()) {
        files = QFileDialog::getOpenFileNames(
            this, tr("Import Files"), QString(),
            tr("Supported (*.pdf *.md *.txt *.png *.jpg *.jpeg *.bmp *.tiff);;All (*)"));
    }

    auto& km = KnowledgeBaseManager::getInstance();
    if (!km.initialize()) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to initialize Knowledge Base"));
        return;
    }

    int added = 0;
    for (const QString& path : files) {
        QFileInfo fi(path);
        KnowledgeEntry entry;
        entry.title = fi.fileName();
        entry.fileType = fi.suffix().toLower();
        entry.sourcePath = path;
        entry.fileSize = fi.size();

        // 读取内容作为 markdown（文本类文件）
        if (entry.fileType == "md" || entry.fileType == "txt") {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                entry.markdownContent = QString::fromUtf8(f.readAll());
                f.close();
            }
        } else {
            // 图片/PDF 只存路径，暂无全文
            entry.markdownContent = QString("Source: %1\nType: %2").arg(path, entry.fileType);
        }

        if (km.addEntry(entry)) added++;
    }

    if (added > 0) {
        emit statusMessage(tr("Archived %1 file(s)").arg(added));
        refreshList();
    }
}

// ============================================================
// View — 弹窗查看 markdown 详情
// ============================================================
void KnowledgeBasePage::onViewEntry(int id) {
    auto doc = KnowledgeBaseManager::getInstance().getEntry(id);
    if (doc.id < 0) return;

    QDialog dlg(this);
    dlg.setWindowTitle(doc.title);
    dlg.setMinimumSize(600, 500);
    dlg.setStyleSheet("QDialog { background: #FFFFFF; border-radius: 8px; }");

    auto* lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(8);

    auto* info = new QLabel(
        tr("Title: %1\nSource: %2\nLanguage: %3\nCreated: %4")
        .arg(doc.title, doc.sourcePath, doc.translatedLang,
             doc.createdAt.toString("yyyy-MM-dd hh:mm")));
    info->setStyleSheet("color: #889096; font-size: 12px;");
    info->setWordWrap(true);
    lay->addWidget(info);

    auto* textEdit = new QPlainTextEdit;
    textEdit->setPlainText(doc.markdownContent);
    textEdit->setReadOnly(true);
    textEdit->setStyleSheet(
        "QPlainTextEdit { border: 1px solid #E8ECEF; border-radius: 6px;"
        " padding: 8px; font-size: 13px; background: #F8FAFA; }");
    lay->addWidget(textEdit, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();

    auto* copyBtn = new QPushButton(tr("Copy All"));
    copyBtn->setStyleSheet(
        "QPushButton { border: 1px solid #D1D5DB; border-radius: 4px;"
        " padding: 4px 16px; background: white; color: #374151; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(copyBtn, &QPushButton::clicked, [&dlg, textEdit]() {
        QApplication::clipboard()->setText(textEdit->toPlainText());
    });
    btnRow->addWidget(copyBtn);

    auto* exportBtn = new QPushButton(tr("Export MD"));
    exportBtn->setObjectName("primaryBtn");
    connect(exportBtn, &QPushButton::clicked, [this, &dlg, id]() {
        onExportEntry(id);
    });
    btnRow->addWidget(exportBtn);

    auto* closeBtn = new QPushButton(tr("Close"));
    closeBtn->setStyleSheet(copyBtn->styleSheet());
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(closeBtn);

    lay->addLayout(btnRow);
    dlg.exec();
}

// ============================================================
// Delete
// ============================================================
void KnowledgeBasePage::onDeleteEntry(int id) {
    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(id);
    if (doc.id < 0) return;

    auto reply = QMessageBox::question(this, tr("Delete"),
                                        tr("Delete \"%1\"?").arg(doc.title),
                                        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        if (km.deleteEntry(id)) {
            emit statusMessage(tr("Deleted: %1").arg(doc.title));
            refreshList();
        }
    }
}

// ============================================================
// Export MD
// ============================================================
void KnowledgeBasePage::onExportEntry(int id) {
    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(id);
    if (doc.id < 0) return;

    QString savePath = QFileDialog::getSaveFileName(
        this, tr("Export Markdown"),
        doc.title + ".md",
        tr("Markdown (*.md)"));
    if (savePath.isEmpty()) return;

    if (km.exportEntry(id, savePath)) {
        emit statusMessage(tr("Exported: %1").arg(savePath));
    } else {
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Could not write to %1").arg(savePath));
    }
}
