#include "knowledgebase_page.h"
#include "knowledgebase_manager.h"
#include "toast.h"
#include "mainwindow.h"        // DropZoneWidget
#include <QFrame>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QDialog>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFileInfo>
#include <QTimer>
#include <QRegularExpression>
#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>

#include "docmind/DocumentEngine.h"
#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/engines/OCREngine.hpp"

#include <opencv2/opencv.hpp>

#include <windows.h>
#include <dbghelp.h>
#include <fstream>
#include <fpdfview.h>
#include <fpdf_edit.h>

// ============================================================
// 全局 VEH — 崩溃时记录堆栈到 D:\crash_log.txt
// ============================================================
#pragma comment(lib, "dbghelp.lib")
static LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep) {
    std::ofstream log("D:\\crash_log.txt", std::ios::app);
    log << "\n=== CRASH at " << GetCurrentProcessId() << " ===" << std::endl;
    log << "ExceptionCode: 0x" << std::hex << ep->ExceptionRecord->ExceptionCode << std::dec << std::endl;
    log << "ExceptionAddress: " << ep->ExceptionRecord->ExceptionAddress << std::endl;

    HANDLE hProcess = GetCurrentProcess();
    SymInitialize(hProcess, NULL, TRUE);
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256, 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    CONTEXT* context = ep->ContextRecord;
    STACKFRAME64 frame = {};
    DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context->Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    for (int i = 0; i < 20; i++) {
        if (!StackWalk64(machine, hProcess, GetCurrentThread(),
                         &frame, context, NULL,
                         SymFunctionTableAccess64, SymGetModuleBase64, NULL))
            break;
        if (frame.AddrPC.Offset == 0) break;
        DWORD64 addr = frame.AddrPC.Offset;
        if (SymFromAddr(hProcess, addr, 0, symbol)) {
            log << "  #" << i << " " << symbol->Name << " +0x" << std::hex
                << (addr - symbol->Address) << std::dec << std::endl;
        } else {
            log << "  #" << i << " 0x" << std::hex << addr << std::dec << std::endl;
        }
    }
    free(symbol);
    SymCleanup(hProcess);
    log.close();
    return EXCEPTION_CONTINUE_SEARCH;
}

namespace {
    struct InstallVEH {
        InstallVEH() { AddVectoredExceptionHandler(1, crashHandler); }
    } _vehInstaller;
}

// ============================================================
// 辅助函数
// ============================================================
namespace {
QPushButton* makeGhostBtn(const QString& text, const QString& tooltip) {
    auto* btn = new QPushButton(text);
    btn->setToolTip(tooltip);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton { border: none; border-radius: 4px; padding: 4px 8px;"
        " background: transparent; color: #6B7280; font-size: 11px; }"
        "QPushButton:hover { background: #F0F7F6; color: #0B7C72; }");
    return btn;
}

QLabel* makeBadge(const QString& text, const QString& bg, const QString& fg) {
    auto* badge = new QLabel(text);
    badge->setStyleSheet(
        QStringLiteral("QLabel { background: %1; color: %2; border-radius: 4px;"
                       " padding: 1px 8px; font-size: 10px; }").arg(bg, fg));
    return badge;
}
}

// ============================================================
// 构造函数
// ============================================================
KnowledgeBasePage::KnowledgeBasePage(QWidget* parent)
    : QWidget(parent) {
    setupUI();
    QTimer::singleShot(0, this, [this]() {
        KnowledgeBaseManager::getInstance().initialize();
        refreshTags();
        refreshList();
    });
}

KnowledgeBasePage::~KnowledgeBasePage() = default;

// ============================================================
// setupUI
// ============================================================
void KnowledgeBasePage::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    // ================================================================
    // 1. 页头 — 标题 + 文档数
    // ================================================================
    auto* headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);

    auto* sectionTitle = new QLabel(tr("Knowledge Base"));
    sectionTitle->setObjectName("sectionTitle");
    sectionTitle->setStyleSheet("font-size: 17px; font-weight: 700; color: #1A1A2E; background: transparent; border: none;");
    headerRow->addWidget(sectionTitle);

    countLabel_ = new QLabel;
    countLabel_->setStyleSheet("font-size: 12px; color: #889096; background: transparent; border: none;");
    headerRow->addWidget(countLabel_);

    headerRow->addStretch();

    auto* viewModeBtn = new QPushButton(tr("View"));
    viewModeBtn->setObjectName("primaryBtn");
    viewModeBtn->setFixedHeight(28);
    headerRow->addWidget(viewModeBtn);

    layout->addLayout(headerRow);

    // ================================================================
    // 2. 上传区
    // ================================================================
    dropZone_ = new DropZoneWidget;
    dropZone_->setFixedHeight(72);
    dropZone_->setStyleSheet(QStringLiteral(
        "DropZoneWidget { background: #F0F7F6; border: 2px dashed #C8E0DC; border-radius: 12px; }"
        "DropZoneWidget:hover { background: #E8F5F3; border-color: #0B7C72; }"));
    auto* dropLayout = new QHBoxLayout(dropZone_);
    dropLayout->setContentsMargins(20, 0, 20, 0);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(12);

    auto* dropIcon = new QLabel(QStringLiteral("\U0001F4C1"));
    dropIcon->setStyleSheet("font-size: 22px; background: transparent;");
    dropLayout->addWidget(dropIcon);

    auto* dropInfo = new QVBoxLayout;
    dropInfo->setContentsMargins(0, 4, 0, 4);
    dropInfo->setSpacing(2);
    auto* dropTitle = new QLabel(tr("Drop files here to archive"));
    dropTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #374151; background: transparent;");
    dropInfo->addWidget(dropTitle);
    auto* dropHint = new QLabel(tr("Supports PDF / DOCX / XLSX / PPTX / MD / TXT / Images"));
    dropHint->setStyleSheet("font-size: 11px; color: #9CA3AF; background: transparent;");
    dropInfo->addWidget(dropHint);
    dropLayout->addLayout(dropInfo);

    auto* badgeRow = new QHBoxLayout;
    badgeRow->setSpacing(4);
    for (const QString& fmt : {"PDF", "DOCX", "XLSX", "PPTX", "MD", "TXT", "IMG"}) {
        auto* badge = new QLabel(fmt);
        badge->setStyleSheet(
            "QLabel { background: #FFFFFF; border: 1px solid #DDE1E5; border-radius: 10px;"
            " padding: 2px 8px; font-size: 10px; font-weight: 500; color: #6B7280; }");
        badgeRow->addWidget(badge);
    }
    dropLayout->addLayout(badgeRow);
    dropLayout->addStretch();

    connect(dropZone_, &DropZoneWidget::fileDropped, this, &KnowledgeBasePage::onFileDropped);
    layout->addWidget(dropZone_);

    // ================================================================
    // 3. 工具栏
    // ================================================================
    auto* toolbar = new QFrame;
    toolbar->setObjectName("card");
    toolbar->setStyleSheet(QStringLiteral("QFrame#card { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 10px; padding: 8px 12px; }"));
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(4, 4, 4, 4);
    tbLayout->setSpacing(6);

    // 搜索框
    searchInput_ = new QLineEdit;
    searchInput_->setPlaceholderText(tr("Search titles, content\u2026"));
    searchInput_->setFixedHeight(30);
    searchInput_->setFixedWidth(170);
    searchInput_->setStyleSheet(
        "QLineEdit { border: 1px solid #DDE1E5; border-radius: 8px;"
        " padding: 0 10px 0 30px; font-size: 12px; background: #F8FAFA; }"
        "QLineEdit:focus { border-color: #0B7C72; background: #FFFFFF; }");
    // search icon overlaid on the left
    auto* searchIcon = new QLabel("\U0001F50D", searchInput_);
    searchIcon->setStyleSheet("font-size: 13px; background: transparent;");
    searchIcon->move(9, 8);
    connect(searchInput_, &QLineEdit::textChanged, this, &KnowledgeBasePage::onSearchTextChanged);
    tbLayout->addWidget(searchInput_, 0);

    // 日期筛选 — 组合在同一个 group 中
    auto* dateGroup = new QFrame;
    dateGroup->setStyleSheet(
        "QFrame { background: #F8FAFA; border: 1px solid #DDE1E5; border-radius: 8px; }");
    auto* dateGLayout = new QHBoxLayout(dateGroup);
    dateGLayout->setContentsMargins(6, 2, 6, 2);
    dateGLayout->setSpacing(4);

    auto* dateIcon = new QLabel("\U0001F4C5");
    dateIcon->setStyleSheet("font-size: 12px; background: transparent;");
    dateGLayout->addWidget(dateIcon);

    dateFrom_ = new QDateEdit;
    dateFrom_->setDisplayFormat("yyyy/MM/dd");
    dateFrom_->setCalendarPopup(true);
    dateFrom_->setFixedHeight(28);
    dateFrom_->setFixedWidth(108);
    dateFrom_->setSpecialValueText("From");
    dateFrom_->setDate(QDate::currentDate().addMonths(-3));
    dateFrom_->setStyleSheet(
        "QDateEdit { border: none; padding: 0 4px; font-size: 12px; background: transparent; }"
        "QDateEdit:focus { background: #FFFFFF; }");
    connect(dateFrom_, &QDateEdit::dateChanged, this, &KnowledgeBasePage::onDateFilterChanged);
    dateGLayout->addWidget(dateFrom_);

    auto* dateSep = new QLabel("\u2013");  // en dash
    dateSep->setStyleSheet("color: #C0C4C8; font-size: 13px; background: transparent;");
    dateGLayout->addWidget(dateSep);

    dateTo_ = new QDateEdit;
    dateTo_->setDisplayFormat("yyyy/MM/dd");
    dateTo_->setCalendarPopup(true);
    dateTo_->setFixedHeight(28);
    dateTo_->setFixedWidth(108);
    dateTo_->setSpecialValueText("To");
    dateTo_->setDate(QDate::currentDate());
    dateTo_->setStyleSheet(
        "QDateEdit { border: none; padding: 0 4px; font-size: 12px; background: transparent; }"
        "QDateEdit:focus { background: #FFFFFF; }");
    connect(dateTo_, &QDateEdit::dateChanged, this, &KnowledgeBasePage::onDateFilterChanged);
    dateGLayout->addWidget(dateTo_);

    auto* clearDateBtn = new QPushButton(tr("Clear"));
    clearDateBtn->setFixedHeight(28);
    clearDateBtn->setCursor(Qt::PointingHandCursor);
    clearDateBtn->setStyleSheet(
        "QPushButton { border: none; border-radius: 4px; padding: 0 6px;"
        " background: transparent; color: #889096; font-size: 11px; }"
        "QPushButton:hover { color: #0B7C72; }");
    connect(clearDateBtn, &QPushButton::clicked, this, [this]() {
        dateFrom_->clear();
        dateTo_->clear();
        refreshList();
    });
    dateGLayout->addWidget(clearDateBtn);

    tbLayout->addWidget(dateGroup, 0);

    // 分隔线
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    sep->setFixedHeight(18);
    sep->setStyleSheet("background: #E8ECEF; border: none;");
    tbLayout->addWidget(sep);

    // 标签筛选
    tagFilterCombo_ = new QComboBox;
    tagFilterCombo_->setFixedHeight(30);
    tagFilterCombo_->setMinimumWidth(100);
    tagFilterCombo_->setStyleSheet(
        "QComboBox { border: 1px solid #DDE1E5; border-radius: 8px;"
        " padding: 0 8px; font-size: 12px; background: #F8FAFA; }"
        "QComboBox:focus { border-color: #0B7C72; background: #FFFFFF; }");
    connect(tagFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &KnowledgeBasePage::onTagFilterChanged);
    tbLayout->addWidget(tagFilterCombo_);

    auto* addTagBtn = new QPushButton(QStringLiteral("+ %1").arg(tr("Tag")));
    addTagBtn->setFixedHeight(30);
    addTagBtn->setStyleSheet(
        "QPushButton { border: 1px solid #DDE1E5; border-radius: 8px; padding: 0 12px;"
        " background: transparent; color: #374151; font-size: 12px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(addTagBtn, &QPushButton::clicked, this, &KnowledgeBasePage::onAddNewTag);
    tbLayout->addWidget(addTagBtn);

    tbLayout->addStretch();

    selectAllBtn_ = new QPushButton(tr("Select All"));
    selectAllBtn_->setFixedHeight(30);
    selectAllBtn_->setStyleSheet(
        "QPushButton { border: 1px solid #DDE1E5; border-radius: 8px; padding: 0 12px;"
        " background: transparent; color: #374151; font-size: 12px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(selectAllBtn_, &QPushButton::clicked, this, &KnowledgeBasePage::onSelectAll);
    tbLayout->addWidget(selectAllBtn_);

    auto* batchBtn = new QPushButton(tr("Batch Import"));
    batchBtn->setObjectName("primaryBtn");
    batchBtn->setFixedHeight(30);
    connect(batchBtn, &QPushButton::clicked, this, &KnowledgeBasePage::onBatchImport);
    tbLayout->addWidget(batchBtn);

    layout->addWidget(toolbar);

    // ================================================================
    // 4. 文档列表（卡片容器）
    // ================================================================
    listCard_ = new QFrame;
    listCard_->setStyleSheet(QStringLiteral(
        "QFrame { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 10px; }"));
    auto* listCardLayout = new QVBoxLayout(listCard_);
    listCardLayout->setContentsMargins(0, 0, 0, 0);
    listCardLayout->setSpacing(0);

    // 列表头部
    auto* listHeader = new QFrame;
    listHeader->setFixedHeight(32);
    listHeader->setStyleSheet("background: #F8FAFA; border-bottom: 1px solid #F0F0F0;");
    auto* listHeaderLayout = new QHBoxLayout(listHeader);
    listHeaderLayout->setContentsMargins(14, 0, 14, 0);
    listHeaderLayout->setSpacing(0);
    auto* colDoc = new QLabel(tr("Document"));
    colDoc->setStyleSheet("font-size: 11px; font-weight: 600; color: #889096; background: transparent; border: none;");
    listHeaderLayout->addWidget(colDoc, 1);
    auto* colDate = new QLabel(tr("Date"));
    colDate->setStyleSheet("font-size: 11px; font-weight: 600; color: #889096; background: transparent; border: none;");
    colDate->setFixedWidth(80);
    colDate->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    listHeaderLayout->addWidget(colDate);
    auto* colTags = new QLabel(tr("Tags"));
    colTags->setStyleSheet("font-size: 11px; font-weight: 600; color: #889096; background: transparent; border: none;");
    colTags->setFixedWidth(140);
    listHeaderLayout->addWidget(colTags);
    auto* colActions = new QLabel(tr("Actions"));
    colActions->setStyleSheet("font-size: 11px; font-weight: 600; color: #889096; background: transparent; border: none;");
    listHeaderLayout->addWidget(colActions);
    listCardLayout->addWidget(listHeader);

    // 滚动区
    listScroll_ = new QScrollArea;
    listScroll_->setWidgetResizable(true);
    listScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    listScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listScroll_->setMinimumHeight(120);
    listScroll_->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 6px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #D0D4D8; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #0B7C72; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    listContainer_ = new QWidget;
    listContainer_->setStyleSheet("QWidget { background: transparent; }");
    listLayout_ = new QVBoxLayout(listContainer_);
    listLayout_->setContentsMargins(8, 4, 8, 4);
    listLayout_->setSpacing(1);

    emptyHint_ = new QLabel;
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setWordWrap(true);
    emptyHint_->setStyleSheet(
        "QLabel { color: #9CA3AF; font-size: 13px; padding: 50px 20px;"
        " background: transparent; border: none; }");
    emptyHint_->setText(
        tr("No documents yet\nDrop files above to get started"));
    listLayout_->addWidget(emptyHint_);
    listLayout_->addStretch();

    listScroll_->setWidget(listContainer_);
    listCardLayout->addWidget(listScroll_, 1);

    layout->addWidget(listCard_, 1);

    // ================================================================
    // 5. 底部批量操作栏
    // ================================================================
    batchBar_ = new QFrame;
    batchBar_->setObjectName("card");
    batchBar_->setStyleSheet(QStringLiteral(
        "QFrame#card { background: #F0F7F6; border: 1px solid #D0E8E4; border-radius: 10px; padding: 0; }"));
    auto* batchLayout = new QHBoxLayout(batchBar_);
    batchLayout->setContentsMargins(14, 8, 14, 8);
    batchLayout->setSpacing(10);

    batchCountLabel_ = new QLabel;
    batchCountLabel_->setStyleSheet("font-size: 12px; color: #374151; background: transparent; border: none;");
    batchLayout->addWidget(batchCountLabel_);
    batchLayout->addStretch();

    batchTagBtn_ = new QPushButton(tr("Batch Tag"));
    batchTagBtn_->setFixedHeight(28);
    batchTagBtn_->setStyleSheet(
        "QPushButton { border: 1px solid #DDE1E5; border-radius: 6px; padding: 0 14px;"
        " background: transparent; color: #374151; font-size: 12px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(batchTagBtn_, &QPushButton::clicked, this, &KnowledgeBasePage::onBatchChangeTags);
    batchLayout->addWidget(batchTagBtn_);

    batchDelBtn_ = new QPushButton(tr("Delete"));
    batchDelBtn_->setFixedHeight(28);
    batchDelBtn_->setStyleSheet(
        "QPushButton { border: 1px solid #FECACA; border-radius: 6px; padding: 0 14px;"
        " background: transparent; color: #DC2626; font-size: 12px; }"
        "QPushButton:hover { background: #FEF2F2; }");
    connect(batchDelBtn_, &QPushButton::clicked, this, &KnowledgeBasePage::onBatchDelete);
    batchLayout->addWidget(batchDelBtn_);

    batchBar_->setVisible(false);
    layout->addWidget(batchBar_);
}

// ============================================================
// refreshList
// ============================================================
void KnowledgeBasePage::refreshList() {
    for (int i = listLayout_->count() - 1; i >= 0; --i) {
        QLayoutItem* item = listLayout_->itemAt(i);
        if (!item) continue;
        if (item->widget() == emptyHint_) continue;
        if (item->spacerItem()) continue;
        if (item->widget()) {
            item->widget()->removeEventFilter(this);
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        listLayout_->takeAt(i);
        delete item;
    }
    checkedDocIds_.clear();
    allSelected_ = false;
    if (selectAllBtn_) selectAllBtn_->setText(tr("Select All"));
    updateBatchBar();

    auto& km = KnowledgeBaseManager::getInstance();
    QList<KnowledgeEntry> docs;
    QString keyword = searchInput_ ? searchInput_->text().trimmed() : QString();
    int tagFilter = tagFilterCombo_ ? tagFilterCombo_->currentData().toInt() : -1;
    QString dateFrom = dateFrom_ && dateFrom_->date().isValid() ? dateFrom_->date().toString("yyyy-MM-dd") : QString();
    QString dateTo = dateTo_ && dateTo_->date().isValid() ? dateTo_->date().toString("yyyy-MM-dd") : QString();
    bool hasDateFilter = !dateFrom.isEmpty() || !dateTo.isEmpty();

    if (!keyword.isEmpty()) {
        docs = km.searchEntries(keyword);
        if (hasDateFilter) {
            QList<KnowledgeEntry> filtered;
            for (const auto& d : docs) {
                QString dStr = d.createdAt.isValid() ? d.createdAt.toString("yyyy-MM-dd") : QString();
                if (!dStr.isEmpty()) {
                    if (!dateFrom.isEmpty() && dStr < dateFrom) continue;
                    if (!dateTo.isEmpty() && dStr > dateTo) continue;
                }
                filtered.append(d);
            }
            docs = filtered;
        }
    } else if (tagFilter > 0) {
        docs = km.getEntriesByTag(tagFilter);
        if (hasDateFilter) {
            QList<KnowledgeEntry> filtered;
            for (const auto& d : docs) {
                QString dStr = d.createdAt.isValid() ? d.createdAt.toString("yyyy-MM-dd") : QString();
                if (!dStr.isEmpty()) {
                    if (!dateFrom.isEmpty() && dStr < dateFrom) continue;
                    if (!dateTo.isEmpty() && dStr > dateTo) continue;
                }
                filtered.append(d);
            }
            docs = filtered;
        }
    } else if (hasDateFilter) {
        docs = km.getEntriesByDate(dateFrom, dateTo);
    } else {
        docs = km.getAllEntries();
    }

    if (countLabel_)
        countLabel_->setText(QStringLiteral("%1 %2").arg(docs.size()).arg(tr("documents")));

    emptyHint_->setVisible(docs.isEmpty());
    for (const auto& doc : docs) {
        auto tags = km.getDocumentTagNames(doc.id);
        QString summary = doc.summary.isEmpty() ? QString() : doc.summary;
        auto* item = createListItem(doc.id, doc.title,
            doc.createdAt.toString("yyyy/MM/dd"), doc.fileType.toUpper(), tags, summary, keyword);
        listLayout_->insertWidget(listLayout_->count() - 1, item);
    }
}

// ============================================================
// createListItem — 行式布局
// ============================================================
QWidget* KnowledgeBasePage::createListItem(int id, const QString& title,
    const QString& date, const QString& fileType,
    const QStringList& tags, const QString& summary, const QString& keyword) {

    // 外层容器
    auto* row = new QFrame;
    row->setProperty("docId", id);
    row->setCursor(Qt::PointingHandCursor);
    row->setStyleSheet(
        "QFrame { background: transparent; border: none; border-radius: 6px; }"
        "QFrame:hover { background: #F8FAFA; }");

    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(10, 6, 10, 6);
    rowLayout->setSpacing(10);

    // ---- CheckBox ----
    auto* checkBox = new QCheckBox;
    checkBox->setProperty("docId", id);
    checkBox->setFixedSize(18, 18);
    checkBox->setStyleSheet(
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px;"
        " border: 1.5px solid #D0D4D8; background: #FFFFFF; }"
        "QCheckBox::indicator:hover { border-color: #0B7C72; }"
        "QCheckBox::indicator:checked { background-color: #0B7C72; border-color: #0B7C72; }"
        "QCheckBox { spacing: 0px; }");
    connect(checkBox, &QCheckBox::toggled, this, [this, id](bool checked) {
        if (checked) checkedDocIds_.insert(id);
        else checkedDocIds_.remove(id);
        updateBatchBar();
    });
    rowLayout->addWidget(checkBox);

    // ---- 文件图标 ----
    QString iconRes = ":/icons/file.png";
    QString ext = fileType.toLower();
    if (ext == "pdf") iconRes = ":/icons/PDF.png";
    else if (ext == "docx") iconRes = ":/icons/DOCX.png";
    else if (ext == "xlsx") iconRes = ":/icons/XLSX.png";
    else if (ext == "pptx") iconRes = ":/icons/PPTX.png";
    else if (ext == "txt") iconRes = ":/icons/TXT.png";
    else if (ext == "md") iconRes = ":/icons/Markdown.png";
    else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tiff")
        iconRes = ":/icons/image.png";

    auto* iconLbl = new QLabel;
    QPixmap pix(iconRes);
    if (!pix.isNull())
        iconLbl->setPixmap(pix.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLbl->setStyleSheet("background: transparent; border: none;");
    rowLayout->addWidget(iconLbl);

    // ---- 标题 + 摘要（弹性列）----
    auto* infoLayout = new QVBoxLayout;
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(3);

    auto highlightText = [](const QString& text, const QString& kw) -> QString {
        if (kw.isEmpty() || text.isEmpty()) return text.toHtmlEscaped();
        QString escaped = text.toHtmlEscaped();
        QRegularExpression re("(" + QRegularExpression::escape(kw) + ")", QRegularExpression::CaseInsensitiveOption);
        return escaped.replace(re, "<span style='background:#FFF3CD;color:#856404;border-radius:2px;padding:0 1px;'>\\1</span>");
    };

    auto* titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(4);

    auto* titleLbl = new QLabel(highlightText(title, keyword));
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setStyleSheet("font-size: 13px; font-weight: 500; color: #1C1C1E; background: transparent; border: none;");
    titleRow->addWidget(titleLbl, 1);

    // 展开/折叠摘要的小按钮
    auto* sumToggle = new QPushButton("▶");
    sumToggle->setFixedSize(18, 18);
    sumToggle->setCursor(Qt::PointingHandCursor);
    sumToggle->setStyleSheet(
        "QPushButton { border: none; border-radius: 3px; font-size: 8px;"
        " color: #9CA3AF; background: transparent; padding: 0px; }"
        "QPushButton:hover { background: #E8F0EF; color: #0B7C72; }");
    titleRow->addWidget(sumToggle);
    infoLayout->addLayout(titleRow);

    // 摘要（默认折叠）
    auto* sumRow = new QWidget;
    sumRow->setVisible(false);
    sumRow->setStyleSheet("background: transparent; border: none;");
    auto* sumRowLayout = new QHBoxLayout(sumRow);
    sumRowLayout->setContentsMargins(0, 0, 0, 0);
    sumRowLayout->setSpacing(8);
    auto* sumBar = new QFrame;
    sumBar->setFixedWidth(3);
    sumBar->setFixedHeight(20);
    sumBar->setStyleSheet("background: #D0E8E4; border-radius: 2px;");
    sumRowLayout->addWidget(sumBar, 0);
    auto* sumLbl = new QLabel(highlightText(summary, keyword));
    sumLbl->setTextFormat(Qt::RichText);
    sumLbl->setWordWrap(true);
    sumLbl->setStyleSheet("font-size: 12px; color: #6B7280; line-height: 1.4; background: transparent; border: none;");
    sumRowLayout->addWidget(sumLbl, 1);
    infoLayout->addWidget(sumRow);

    connect(sumToggle, &QPushButton::clicked, sumRow, [sumToggle, sumRow]() {
        bool vis = sumRow->isVisible();
        sumRow->setVisible(!vis);
        sumToggle->setText(vis ? "▶" : "▼");
    });

    // 如果摘要匹配搜索关键词，默认展开
    if (!summary.isEmpty() && !keyword.isEmpty() && summary.contains(keyword, Qt::CaseInsensitive)) {
        sumRow->setVisible(true);
        sumToggle->setText("▼");
    }

    rowLayout->addLayout(infoLayout, 1);

    // ---- 日期 ----
    auto* dateLbl = new QLabel(date);
    dateLbl->setStyleSheet("font-size: 11px; color: #9CA3AF; background: transparent; border: none; white-space: nowrap;");
    dateLbl->setFixedWidth(80);
    dateLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowLayout->addWidget(dateLbl);

    // ---- 标签 ----
    auto* tagContainer = new QWidget;
    tagContainer->setStyleSheet("background: transparent; border: none;");
    auto* tagFlow = new QHBoxLayout(tagContainer);
    tagFlow->setContentsMargins(0, 0, 0, 0);
    tagFlow->setSpacing(4);
    int shown = 0;
    for (const QString& t : tags) {
        if (shown++ >= 2) break;
        auto* tagBadge = new QLabel(t);
        tagBadge->setStyleSheet(
            "QLabel { background: #F0F7F6; color: #0B7C72; border-radius: 4px;"
            " padding: 1px 8px; font-size: 10px; }");
        tagFlow->addWidget(tagBadge);
    }
    if (tags.size() > 2) {
        auto* more = new QLabel(QStringLiteral("+%1").arg(tags.size() - 2));
        more->setStyleSheet("font-size: 10px; color: #9CA3AF; background: transparent; border: none;");
        tagFlow->addWidget(more);
    }
    tagContainer->setFixedWidth(140);
    rowLayout->addWidget(tagContainer);

    // ---- 操作按钮 ----
    auto* actions = new QWidget;
    actions->setStyleSheet("background: transparent; border: none;");
    auto* actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(2);

    auto* viewBtn = makeGhostBtn(tr("View"), tr("Open source file"));
    auto* delBtn = makeGhostBtn("\u2715", tr("Delete"));

    actionLayout->addWidget(viewBtn);
    actionLayout->addWidget(delBtn);
    rowLayout->addWidget(actions);

    connect(viewBtn, &QPushButton::clicked, this, [this, id]() { showDocumentDetail(id); });
    connect(delBtn, &QPushButton::clicked, this, [this, id, row]() {
        auto reply = QMessageBox::question(this, tr("Delete"),
            tr("Delete this document?"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        KnowledgeBaseManager::getInstance().deleteEntry(id);
        row->hide();
        row->deleteLater();
        refreshList();
    });

    // 双击打开源文件
    row->installEventFilter(this);
    row->setProperty("_kb_docId", id);
    row->setProperty("_kb_doubleClick", true);

    return row;
}

// ============================================================
// eventFilter
// ============================================================
bool KnowledgeBasePage::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (!w) return QWidget::eventFilter(obj, event);
        if (w->property("_kb_doubleClick").toBool()) {
            int docId = w->property("_kb_docId").toInt();
            if (docId > 0) { showDocumentDetail(docId); return true; }
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ============================================================
// showDocumentDetail — 双击用系统默认程序打开源文件
// ============================================================
void KnowledgeBasePage::showDocumentDetail(int docId) {
    auto& km = KnowledgeBaseManager::getInstance();
    auto entry = km.getEntry(docId);
    if (entry.id < 0) return;
    QString filePath = entry.sourcePath;
    if (filePath.isEmpty()) { emit statusMessage(tr("Source path is empty")); return; }
    QFileInfo fi(filePath);
    if (!fi.exists()) { emit statusMessage(tr("File not found: %1").arg(filePath)); return; }
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

// ============================================================
// onFileDropped
// ============================================================
void KnowledgeBasePage::onFileDropped(const QStringList& paths) {
    if (isImporting_) {
        emit statusMessage(tr("Import in progress, please wait"));
        return;
    }
    static bool processing = false;
    if (processing) return;
    processing = true;

    QStringList files = paths;
    if (files.isEmpty()) {
        files = QFileDialog::getOpenFileNames(this, tr("Select Files"), QString(),
            tr("Supported files (*.pdf *.docx *.xlsx *.pptx *.md *.txt *.png *.jpg *.jpeg *.bmp *.tiff);;All files (*)"));
    }
    if (files.isEmpty()) { processing = false; return; }
    files.removeDuplicates();

    auto& km = KnowledgeBaseManager::getInstance();
    km.initialize();

    QList<ImportTask> tasks;
    for (const QString& path : files) {
        QFileInfo fi(path);
        ImportTask t;
        t.filePath = path;
        t.title = fi.fileName();
        t.fileType = fi.suffix().toLower();
        t.fileSize = fi.size();
        tasks.append(t);
    }

    int total = tasks.size();
    setEnabled(false);
    emit statusMessage(QStringLiteral("%1 %2").arg(total).arg(tr("files parsing\u2026")));
    QString baseDir = QCoreApplication::applicationDirPath();
    importGeneration_++;
    pendingTasks_ = tasks;
    pendingBaseDir_ = baseDir;
    processIndex_ = 0;
    importCount_ = 0;
    isImporting_ = true;
    int myGen = importGeneration_;

    QTimer::singleShot(100, this, [this, myGen]() {
        if (myGen != importGeneration_) return;
        processNextFile(myGen);
        processing = false;
    });
}

// ============================================================
// extract_image_text
// ============================================================
std::string KnowledgeBasePage::extract_image_text(const std::string& image_path) {
    cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);
    if (img.empty()) return "";
    auto& ctx = docmind::GlobalEngineContext::getInstance();
    if (!ctx.ensureOCREngine()) return "";
    if (auto* engine = ctx.getOCREngine()) {
        auto results = engine->recognizeBuffer(img);
        std::string text;
        for (const auto& r : results) {
            if (!r.text.empty()) text += r.text + "\n";
        }
        return text;
    }
    return "";
}

// ============================================================
// extract_pdf_text
// ============================================================
std::string KnowledgeBasePage::extract_pdf_text(const std::string& pdf_path, const std::string& base_dir, int dpi) {
    std::vector<cv::Mat> pages;
    {
        std::ifstream file(pdf_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return "";
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        file.close();

        FPDF_DOCUMENT doc = FPDF_LoadMemDocument(data.data(), static_cast<int>(size), nullptr);
        if (!doc) return "";
        int page_count = FPDF_GetPageCount(doc);
        for (int i = 0; i < page_count; ++i) {
            FPDF_PAGE page = FPDF_LoadPage(doc, i);
            if (!page) continue;
            int w = static_cast<int>(FPDF_GetPageWidth(page) * dpi / 72);
            int h = static_cast<int>(FPDF_GetPageHeight(page) * dpi / 72);
            FPDF_BITMAP bitmap = FPDFBitmap_Create(w, h, 0);
            FPDFBitmap_FillRect(bitmap, 0, 0, w, h, 0xFFFFFFFF);
            FPDF_RenderPageBitmap(bitmap, page, 0, 0, w, h, 0, 0);
            unsigned char* buf = static_cast<unsigned char*>(FPDFBitmap_GetBuffer(bitmap));
            cv::Mat mat(h, w, CV_8UC4, buf);
            cv::Mat rgb;
            cv::cvtColor(mat, rgb, cv::COLOR_BGRA2BGR);
            pages.push_back(rgb);
            FPDFBitmap_Destroy(bitmap);
            FPDF_ClosePage(page);
        }
        FPDF_CloseDocument(doc);
    }

    if (pages.empty()) return "";
    auto& ctx = docmind::GlobalEngineContext::getInstance();
    if (!ctx.ensureOCREngine()) return "";
    if (auto* engine = ctx.getOCREngine()) {
        std::string full_text;
        for (const auto& page : pages) {
            auto results = engine->recognizeBuffer(page);
            for (const auto& r : results) {
                if (!r.text.empty()) full_text += r.text + "\n";
            }
        }
        return full_text;
    }
    return "";
}

// ============================================================
// processNextFile
// ============================================================
void KnowledgeBasePage::processNextFile(int myGen) {
    if (myGen != importGeneration_ || !isImporting_) return;

    if (processIndex_ >= pendingTasks_.size()) {
        pendingTasks_.clear();
        refreshList();
        setEnabled(true);
        emit statusMessage(QStringLiteral("%1 %2").arg(importCount_).arg(tr("documents imported")));
        ToastNotification::show(this, QStringLiteral("已导入 %1 个文档").arg(importCount_), 4000, QColor(11, 124, 114));
        importCount_ = 0;
        processIndex_ = 0;
        isImporting_ = false;
        return;
    }

    const ImportTask& task = pendingTasks_[processIndex_];
    emit statusMessage(QStringLiteral("%1/%2 %3\u2026").arg(processIndex_ + 1).arg(pendingTasks_.size()).arg(tr("parsing")));

    QString ext = task.fileType;
    if (ext == "md" || ext == "txt") {
        QString markdown;
        QFile f(task.filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            markdown = QString::fromUtf8(f.readAll());
            f.close();
        }
        processIndex_++;
        finishEntry(task, markdown);
        QTimer::singleShot(0, this, [this, myGen]() { processNextFile(myGen); });
    } else {
        QString filePath = task.filePath;
        QString fileName = task.title;
        QString fileType = task.fileType;
        qint64 fileSize = task.fileSize;
        processIndex_++;

        auto onFinish = [this, fileName, fileType, filePath, fileSize, myGen](QString markdown) {
            ImportTask t;
            t.filePath = filePath;
            t.title = fileName;
            t.fileType = fileType;
            t.fileSize = fileSize;
            finishEntry(t, markdown);
            QTimer::singleShot(0, this, [this, myGen]() { processNextFile(myGen); });
        };

        std::thread([onFinish, filePath, fileType]() {
            QString md;
            try {
                std::string baseDir = QCoreApplication::applicationDirPath().toStdString();
                auto& cfg = docmind::ConfigManager::getInstance();
                float threshold = cfg.getNestedJson("defaults").value("layout_threshold", nlohmann::json(0.5)).get<float>();
                int dpi = static_cast<int>(cfg.getNestedJson("defaults").value("pdf_dpi", nlohmann::json(200)).get<double>());
                auto defaults = cfg.getNestedJson("defaults");
                bool enable_warp = defaults.value("enable_warp", nlohmann::json(true)).get<bool>();
                bool enable_enhance = defaults.value("enable_enhance", nlohmann::json(true)).get<bool>();
                std::string extracted = extract_file_text(
                    filePath.toStdString(), "", baseDir,
                    threshold, dpi, enable_warp, enable_enhance);
                if (!extracted.empty()) md = QString::fromStdString(extracted);
            } catch (...) {}
            QMetaObject::invokeMethod(QCoreApplication::instance(), [onFinish, md]() {
                onFinish(md);
            }, Qt::QueuedConnection);
        }).detach();
    }
}

void KnowledgeBasePage::onBatchImport() { onFileDropped(QStringList()); }

// ============================================================
// finishEntry — 入库 + 后台生成摘要
// ============================================================
void KnowledgeBasePage::finishEntry(const ImportTask& task, const QString& markdown) {
    auto& km = KnowledgeBaseManager::getInstance();
    KnowledgeEntry entry;
    entry.title = task.title;
    entry.fileType = task.fileType;
    entry.sourcePath = task.filePath;
    entry.fileSize = task.fileSize;
    entry.markdownContent = markdown;
    int newId = -1;
    if (km.addEntry(entry, &newId)) {
        importCount_++;
        if (!markdown.trimmed().isEmpty()) {
            QString savedMarkdown = markdown;
            int savedId = newId;
            std::thread([this, savedMarkdown, savedId]() {
                QString summary;
                auto& ctx = docmind::GlobalEngineContext::getInstance();
                if (ctx.ensureSummarizerEngine()) {
                    if (auto* engine = ctx.getSummarizerEngine()) {
                        std::string text_utf8 = savedMarkdown.toUtf8().constData();
                        if (text_utf8.length() > 4000) text_utf8 = text_utf8.substr(0, 4000);
                        std::string aiResult = engine->summarize(text_utf8, 4096);
                        if (!aiResult.empty()) summary = QString::fromUtf8(aiResult.c_str());
                    }
                }
                if (summary.isEmpty()) {
                    QString plain = savedMarkdown.simplified();
                    if (plain.length() > 2000) plain = plain.left(2000) + "\u2026";
                    summary = plain.left(200);
                    if (plain.length() > 200) summary += "\u2026";
                }
                QMetaObject::invokeMethod(this, [this, savedId, summary]() {
                    onSummaryReady(savedId, summary);
                }, Qt::QueuedConnection);
            }).detach();
        }
    }
}

void KnowledgeBasePage::onSummaryReady(int docId, const QString& summary) {
    KnowledgeBaseManager::getInstance().updateSummary(docId, summary);
    refreshList();
}

// ============================================================
// 工具栏回调
// ============================================================
void KnowledgeBasePage::onSearchTextChanged(const QString&) {
    static QTimer* debounce = nullptr;
    if (!debounce) { debounce = new QTimer(this); debounce->setSingleShot(true); connect(debounce, &QTimer::timeout, this, &KnowledgeBasePage::refreshList); }
    debounce->start(300);
}
void KnowledgeBasePage::onTagFilterChanged(int) { refreshList(); }
void KnowledgeBasePage::onDateFilterChanged() { refreshList(); }
void KnowledgeBasePage::onAddNewTag() {
    bool ok;
    QString name = QInputDialog::getText(this, tr("New Tag"), tr("Tag name:"), QLineEdit::Normal, {}, &ok);
    if (ok && !name.trimmed().isEmpty()) {
        auto& km = KnowledgeBaseManager::getInstance();
        if (km.addTag(name.trimmed())) { refreshTags(); emit statusMessage(tr("Tag created: %1").arg(name.trimmed())); }
        else emit statusMessage(tr("Tag already exists"));
    }
}
void KnowledgeBasePage::refreshTags() {
    if (!tagFilterCombo_) return;
    tagFilterCombo_->blockSignals(true);
    tagFilterCombo_->clear();
    tagFilterCombo_->addItem(tr("All Documents"), -1);
    for (const auto& t : KnowledgeBaseManager::getInstance().getAllTags())
        tagFilterCombo_->addItem(t.second, t.first);
    tagFilterCombo_->blockSignals(false);
}

// ============================================================
// 底部批量操作栏
// ============================================================
void KnowledgeBasePage::updateBatchBar() {
    int n = checkedDocIds_.size();
    batchCountLabel_->setText(QStringLiteral("%1 %2").arg(n).arg(tr("selected")));
    batchBar_->setVisible(n > 0);
}

void KnowledgeBasePage::onSelectAll() {
    allSelected_ = !allSelected_;
    if (allSelected_) {
        auto& km = KnowledgeBaseManager::getInstance();
        QString keyword = searchInput_ ? searchInput_->text().trimmed() : QString();
        int tagFilter = tagFilterCombo_ ? tagFilterCombo_->currentData().toInt() : -1;
        QList<KnowledgeEntry> docs;
        if (!keyword.isEmpty()) docs = km.searchEntries(keyword);
        else if (tagFilter > 0) docs = km.getEntriesByTag(tagFilter);
        else docs = km.getAllEntries();
        for (const auto& doc : docs) checkedDocIds_.insert(doc.id);
        selectAllBtn_->setText(tr("Deselect All"));
    } else {
        checkedDocIds_.clear();
        selectAllBtn_->setText(tr("Select All"));
    }
    for (QCheckBox* cb : findChildren<QCheckBox*>()) {
        if (cb->property("docId").isValid()) cb->setChecked(allSelected_);
    }
    updateBatchBar();
}

void KnowledgeBasePage::onBatchDelete() {
    if (checkedDocIds_.isEmpty()) return;
    auto reply = QMessageBox::question(this, tr("Batch Delete"),
        QStringLiteral("%1 %2?").arg(checkedDocIds_.size()).arg(tr("documents to delete")),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    auto& km = KnowledgeBaseManager::getInstance();
    QSet<int> ids = checkedDocIds_;
    int deleted = km.deleteEntries(QList<int>(ids.begin(), ids.end()));
    emit statusMessage(QStringLiteral("%1 %2").arg(deleted).arg(tr("documents deleted")));
    refreshList();
}

void KnowledgeBasePage::onBatchChangeTags() {
    if (checkedDocIds_.isEmpty()) return;
    auto& km = KnowledgeBaseManager::getInstance();
    auto allTags = km.getAllTags();
    if (allTags.isEmpty()) {
        QMessageBox::information(this, tr("Batch Tag"), tr("No tags yet, create one first"));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("%1 (%2)").arg(tr("Batch Tag")).arg(checkedDocIds_.size()));
    dlg.setMinimumWidth(300);
    auto* lay = new QVBoxLayout(&dlg);
    QList<QCheckBox*> checks;
    for (const auto& t : allTags) {
        auto* cb = new QCheckBox(t.second);
        checks.append(cb);
        lay->addWidget(cb);
    }
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btnBox);

    if (dlg.exec() == QDialog::Accepted) {
        QList<int> selected;
        for (int i = 0; i < checks.size(); ++i)
            if (checks[i]->isChecked()) selected.append(allTags[i].first);
        for (int id : checkedDocIds_)
            km.setDocumentTags(id, selected);
        emit statusMessage(QStringLiteral("%1 %2").arg(checkedDocIds_.size()).arg(tr("documents tagged")));
        refreshList();
    }
}
