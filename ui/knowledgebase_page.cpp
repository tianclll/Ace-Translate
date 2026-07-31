#include "knowledgebase_page.h"
#include "knowledgebase_manager.h"
#include "toast.h"
#include "mainwindow.h"        // DropZoneWidget
#include <QFrame>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QDialog>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QToolButton>
#include <QFileInfo>
#include <QTimer>
#include <QRegularExpression>
#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPixmap>
#include <functional>

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

/// 递归复制目录 srcDir → dstDir（目标已存在的同名文件先删除再复制，支持覆盖）
bool copyDirectoryRecursive(const QString& srcDir, const QString& dstDir) {
    QDir src(srcDir);
    if (!src.exists()) return false;
    QDir dst(dstDir);
    if (!dst.exists())
        if (!dst.mkpath(".")) return false;

    const QStringList files = src.entryList(QDir::Files | QDir::NoSymLinks);
    for (const QString& f : files) {
        QString target = dst.absoluteFilePath(f);
        if (QFile::exists(target)) QFile::remove(target);
        if (!QFile::copy(src.absoluteFilePath(f), target))
            return false;
    }
    const QStringList subDirs = src.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for (const QString& d : subDirs) {
        if (!copyDirectoryRecursive(src.absoluteFilePath(d), dst.absoluteFilePath(d)))
            return false;
    }
    return true;
}

/// 创建无边框圆角弹窗：返回 dialog 与白色圆角容器，并绑定自适应尺寸逻辑。
/// content 需自己填充；最后调用 applyDialogSize() 让其按内容自适应（超高时由内部滚动区承接）。
struct RoundedDlg {
    QDialog* dlg = nullptr;
    QWidget* container = nullptr;
    QVBoxLayout* lay = nullptr;

    RoundedDlg(QWidget* parent) {
        dlg = new QDialog(parent);
        dlg->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dlg->setAttribute(Qt::WA_TranslucentBackground);
        dlg->setModal(true);

        container = new QWidget(dlg);
        container->setObjectName("kbRoundedDlg");
        container->setStyleSheet(
            "QWidget#kbRoundedDlg { background: #FFFFFF; border-radius: 14px; }");
        auto* outer = new QVBoxLayout(dlg);
        outer->setContentsMargins(16, 16, 16, 16);
        outer->addWidget(container);

        lay = new QVBoxLayout(container);
        lay->setContentsMargins(20, 18, 20, 16);
        lay->setSpacing(10);
    }

    /// 按内容自适应宽高，并在所属顶层窗口内居中；超高时夹紧高度，由内部滚动区承接。
    void applyDialogSize(int minW = 320) {
        // 找到所属顶层窗口，以其全局几何为基准定位
        QWidget* win = dlg;
        while (win->parentWidget()) win = win->parentWidget();

        QSize sh = dlg->layout()->sizeHint();
        int w = qMax(sh.width(), minW);
        int h = qMin(sh.height(), qMax(260, win->height() - 120));
        int maxW = win->width() - 48;
        dlg->resize(qMin(w, maxW), h);

        QRect g = win->geometry();
        dlg->move(g.x() + g.width() / 2 - dlg->width() / 2,
                   g.y() + g.height() / 2 - dlg->height() / 2);
    }
};
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
    // 1. 上传区
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
    // 2. 工具栏
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
    auto* searchIcon = new QLabel(searchInput_);
    QPixmap searchPix(QStringLiteral(":/icons/Search.png"));
    if (!searchPix.isNull())
        searchIcon->setPixmap(searchPix.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    searchIcon->setStyleSheet("background: transparent;");
    searchIcon->move(9, 8);
    connect(searchInput_, &QLineEdit::returnPressed, this, &KnowledgeBasePage::refreshList);
    tbLayout->addWidget(searchInput_, 0);

    // 日期筛选 — 组合在同一个 group 中
    auto* dateGroup = new QFrame;
    dateGroup->setStyleSheet(
        "QFrame { background: #F8FAFA; border: none; }");
    auto* dateGLayout = new QHBoxLayout(dateGroup);
    dateGLayout->setContentsMargins(10, 2, 10, 2);
    dateGLayout->setSpacing(6);

    auto* dateIcon = new QLabel;
    QPixmap datePix(QStringLiteral(":/icons/Calendar.png"));
    if (!datePix.isNull())
        dateIcon->setPixmap(datePix.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    dateIcon->setStyleSheet("background: transparent;");
    dateGLayout->addWidget(dateIcon);

    dateFrom_ = new QDateTimeEdit;
    dateFrom_->setDisplayFormat("yyyy/MM/dd hh:mm");
    dateFrom_->setCalendarPopup(true);
    dateFrom_->setFixedHeight(30);
    dateFrom_->setFixedWidth(150);
    dateFrom_->setSpecialValueText(tr("From"));
    dateFrom_->setDateTime(QDateTime::currentDateTime().addMonths(-3));
    dateFrom_->setStyleSheet(
        "QDateTimeEdit { padding: 0 6px 0 10px; font-size: 12px; background: transparent; }"
        "QDateTimeEdit::drop-down { border: none; width: 0px; height: 0px; margin: 0px; padding: 0px; }"
        "QDateTimeEdit::down-arrow { image: none; width: 0; height: 0; }");
    // 箭头被隐藏后，点击输入框也要能弹出日历（行和内部 lineEdit 都要装 filter）
    dateFrom_->installEventFilter(this);
    if (auto* le = dateFrom_->findChild<QLineEdit*>())
        le->installEventFilter(this);
    connect(dateFrom_, &QDateTimeEdit::dateTimeChanged, this, &KnowledgeBasePage::onDateFilterChanged);
    dateGLayout->addWidget(dateFrom_);

    auto dateFieldStyle =
        "QDateTimeEdit { padding: 0 6px 0 10px; font-size: 12px; background: transparent; }"
        "QDateTimeEdit::drop-down { border: none; width: 0px; height: 0px; margin: 0px; padding: 0px; }"
        "QDateTimeEdit::down-arrow { image: none; width: 0; height: 0; }";

    auto* dateSep = new QLabel("–");  // en dash（不需要 To 文字，只保留分隔线）
    dateSep->setStyleSheet("color: #C0C4C8; font-size: 13px; background: transparent;");
    dateGLayout->addWidget(dateSep);

    dateTo_ = new QDateTimeEdit;
    dateTo_->setDisplayFormat("yyyy/MM/dd hh:mm");
    dateTo_->setCalendarPopup(true);
    dateTo_->setFixedHeight(30);
    dateTo_->setFixedWidth(150);
    dateTo_->setSpecialValueText(tr("To"));
    dateTo_->setDateTime(QDateTime::currentDateTime());
    dateTo_->setStyleSheet(dateFieldStyle);
    dateTo_->installEventFilter(this);
    if (auto* le = dateTo_->findChild<QLineEdit*>())
        le->installEventFilter(this);
    connect(dateTo_, &QDateTimeEdit::dateTimeChanged, this, &KnowledgeBasePage::onDateFilterChanged);
    dateGLayout->addWidget(dateTo_);

    auto* clearDateBtn = new QPushButton(tr("Clear"));
    clearDateBtn->setFixedHeight(30);
    clearDateBtn->setCursor(Qt::PointingHandCursor);
    clearDateBtn->setStyleSheet(
        "QPushButton { border: none; border-radius: 4px; padding: 0 6px;"
        " background: transparent; color: #889096; font-size: 12px; }"
        "QPushButton:hover { color: #0B7C72; }");
    connect(clearDateBtn, &QPushButton::clicked, this, [this]() {
        dateFrom_->blockSignals(true);
        dateTo_->blockSignals(true);
        dateFrom_->setDateTime(QDateTime::currentDateTime().addMonths(-3));
        dateTo_->setDateTime(QDateTime::currentDateTime());
        dateFrom_->blockSignals(false);
        dateTo_->blockSignals(false);
        refreshList();
    });
    dateGLayout->addWidget(clearDateBtn);

    tbLayout->addWidget(dateGroup, 0);

    // 搜索按钮（点击/回车触发搜索，放在日期之后）
    auto* searchBtn = new QPushButton(tr("Search"));
    searchBtn->setObjectName("primaryBtn");
    searchBtn->setFixedHeight(30);
    connect(searchBtn, &QPushButton::clicked, this, &KnowledgeBasePage::refreshList);
    tbLayout->addWidget(searchBtn);

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

    // 新建标签
    auto* addTagBtn = new QPushButton(QStringLiteral("+ %1").arg(tr("Tag")));
    addTagBtn->setFixedHeight(30);
    addTagBtn->setCursor(Qt::PointingHandCursor);
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

    layout->addWidget(toolbar);

    // ================================================================
    // 3. 文档列表（卡片容器）
    // ================================================================
    listCard_ = new QFrame;
    listCard_->setStyleSheet(QStringLiteral(
        "QFrame { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 10px; }"));
    auto* listCardLayout = new QVBoxLayout(listCard_);
    listCardLayout->setContentsMargins(0, 0, 0, 0);
    listCardLayout->setSpacing(0);

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
    listLayout_->setContentsMargins(0, 0, 0, 0);
    listLayout_->setSpacing(1);

    // 表头（放滚动区内第 0 项，与数据行共享相同的横向结构以对齐）
    listHeader_ = new QFrame;
    listHeader_->setObjectName("kbListHeader");
    listHeader_->setFixedHeight(32);
    listHeader_->setStyleSheet(
        "QFrame#kbListHeader { background: #F8FAFA; border-bottom: 1px solid #F0F0F0; }");
    auto* listHeaderLayout = new QHBoxLayout(listHeader_);
    listHeaderLayout->setContentsMargins(10, 0, 10, 0);
    listHeaderLayout->setSpacing(14);

    auto makeHeaderStub = []() {
        auto* stub = new QLabel;
        stub->setFixedHeight(1);
        stub->setStyleSheet("background: transparent; border: none;");
        return stub;
    };

    // 占位：与数据行的 checkbox(18) + spacing + icon(22) 对齐
    auto* headerStubCheck = makeHeaderStub();
    headerStubCheck->setFixedWidth(18);
    listHeaderLayout->addWidget(headerStubCheck);
    auto* headerStubIcon = makeHeaderStub();
    headerStubIcon->setFixedWidth(22);
    listHeaderLayout->addWidget(headerStubIcon);

    auto headerItemStyle = "font-size: 11px; font-weight: 600; color: #889096;"
                           "background: transparent; border: none;";
    auto* colDoc = new QLabel(tr("Document"));
    colDoc->setStyleSheet(headerItemStyle);
    listHeaderLayout->addWidget(colDoc, 1);

    auto* colDate = new QLabel(tr("Date"));
    colDate->setStyleSheet(headerItemStyle);
    colDate->setFixedWidth(120);
    colDate->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    listHeaderLayout->addWidget(colDate);

    auto* colTags = new QLabel(tr("Tags"));
    colTags->setStyleSheet(headerItemStyle);
    colTags->setFixedWidth(150);
    colTags->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    listHeaderLayout->addWidget(colTags);

    auto* colActions = new QLabel(tr("Actions"));
    colActions->setStyleSheet(headerItemStyle);
    colActions->setFixedWidth(90);
    colActions->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    listHeaderLayout->addWidget(colActions);

    listLayout_->addWidget(listHeader_);

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
    // 4. 底部批量操作栏
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

    batchExportBtn_ = new QPushButton(tr("Export"));
    batchExportBtn_->setFixedHeight(28);
    batchExportBtn_->setCursor(Qt::PointingHandCursor);
    batchExportBtn_->setStyleSheet(
        "QPushButton { border: 1px solid #DDE1E5; border-radius: 6px; padding: 0 14px;"
        " background: transparent; color: #374151; font-size: 12px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(batchExportBtn_, &QPushButton::clicked, this, &KnowledgeBasePage::onBatchExport);
    batchLayout->addWidget(batchExportBtn_);

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
        if (item->widget() == listHeader_) continue;
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
    QString dateFrom = dateFrom_ && dateFrom_->dateTime().isValid() ? dateFrom_->dateTime().toString("yyyy-MM-dd hh:mm:ss") : QString();
    QString dateTo = dateTo_ && dateTo_->dateTime().isValid() ? dateTo_->dateTime().toString("yyyy-MM-dd hh:mm:ss") : QString();
    bool hasDateFilter = !dateFrom.isEmpty() || !dateTo.isEmpty();

    if (!keyword.isEmpty()) {
        docs = km.searchEntries(keyword);
        if (hasDateFilter) {
            QList<KnowledgeEntry> filtered;
            for (const auto& d : docs) {
                QString dStr = d.createdAt.isValid() ? d.createdAt.toString("yyyy-MM-dd hh:mm:ss") : QString();
                if (!dStr.isEmpty()) {
                    if (!dateFrom.isEmpty() && dStr < dateFrom) continue;
                    if (!dateTo.isEmpty() && dStr > dateTo) continue;
                }
                filtered.append(d);
            }
            docs = filtered;
        }
    } else if (tagFilter == -2) {
        // 只看「无标签」的文档
        docs = km.getAllEntries();
        QList<KnowledgeEntry> notagged;
        for (const auto& d : docs)
            if (km.getDocumentTagNames(d.id).isEmpty()) notagged.append(d);
        docs = notagged;
        if (hasDateFilter) {
            QList<KnowledgeEntry> filtered;
            for (const auto& d : docs) {
                QString dStr = d.createdAt.isValid() ? d.createdAt.toString("yyyy-MM-dd hh:mm:ss") : QString();
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
                QString dStr = d.createdAt.isValid() ? d.createdAt.toString("yyyy-MM-dd hh:mm:ss") : QString();
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
    rowLayout->setSpacing(14);

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

    // ---- 日期（独立列，居中，宽 120 与表头 Date 对齐，容纳较长日期）----
    auto* dateLbl = new QLabel(date);
    dateLbl->setStyleSheet("font-size: 11px; color: #9CA3AF; background: transparent; border: none; white-space: nowrap;");
    dateLbl->setFixedWidth(120);
    dateLbl->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    rowLayout->addWidget(dateLbl);

    // ---- 标签（独立列，居中流式，宽 150 与表头 Tags 对齐）----
    auto* tagContainer = new QWidget;
    tagContainer->setStyleSheet("background: transparent; border: none;");
    tagContainer->setFixedWidth(150);
    auto* tagFlow = new QHBoxLayout(tagContainer);
    tagFlow->setContentsMargins(0, 0, 0, 0);
    tagFlow->setSpacing(4);
    tagFlow->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
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
    rowLayout->addWidget(tagContainer);

    // ---- 操作（独立列，居中，宽 90 与表头 Actions 对齐）----
    auto* actions = new QWidget;
    actions->setFixedWidth(90);
    actions->setStyleSheet("background: transparent; border: none;");
    auto* actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(2);
    actionLayout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    auto* viewBtn = makeGhostBtn(tr("View"), tr("Open source file"));
    auto* delBtn = makeGhostBtn("✕", tr("Delete"));

    actionLayout->addWidget(viewBtn);
    actionLayout->addWidget(delBtn);
    rowLayout->addWidget(actions);

    connect(viewBtn, &QPushButton::clicked, this, [this, id]() { showDocumentDetail(id); });
    connect(delBtn, &QPushButton::clicked, this, [this, id, row]() {
        RoundedDlg rd(this);
        auto* dlg = rd.dlg;
        auto* lay = rd.lay;

        auto* title = new QLabel(tr("Delete"));
        title->setStyleSheet("font-size: 15px; font-weight: 600; color: #1C1C1E; background: transparent;");
        lay->addWidget(title);

        auto* hint = new QLabel(tr("Delete this document?"));
        hint->setWordWrap(true);
        hint->setStyleSheet("font-size: 12px; color: #374151; background: transparent;");
        lay->addWidget(hint);

        lay->addSpacing(6);

        auto* btnRow = new QHBoxLayout;
        btnRow->setContentsMargins(0, 4, 0, 0);
        btnRow->setSpacing(8);
        btnRow->addStretch();
        auto* cancelBtn = new QPushButton(tr("Cancel"));
        cancelBtn->setObjectName("secondaryBtn");
        cancelBtn->setFixedHeight(30);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
        btnRow->addWidget(cancelBtn);
        auto* delBtn2 = new QPushButton(tr("Delete"));
        delBtn2->setObjectName("primaryBtn");
        delBtn2->setFixedHeight(30);
        delBtn2->setCursor(Qt::PointingHandCursor);
        connect(delBtn2, &QPushButton::clicked, dlg, &QDialog::accept);
        btnRow->addWidget(delBtn2);
        lay->addLayout(btnRow);

        rd.applyDialogSize(300);

        if (dlg->exec() != QDialog::Accepted) return;
        KnowledgeBaseManager::getInstance().deleteEntry(id);
        row->hide();
        row->deleteLater();
        refreshList();
    });

    // 单击选中，双击取消选中
    row->setProperty("_kb_rowClick", id);
    row->installEventFilter(this);

    // 子控件拦截点击事件，通过父链找到行的 _kb_rowClick
    const QList<QWidget*> children = row->findChildren<QWidget*>();
    for (QWidget* child : children) {
        if (child != row)
            child->installEventFilter(this);
    }

    return row;
}

// ============================================================
// eventFilter — 日历框点击弹出日历 / 行单击 toggle 选中
// ============================================================
bool KnowledgeBasePage::eventFilter(QObject* obj, QEvent* event) {
    auto* w = qobject_cast<QWidget*>(obj);
    if (!w) return QWidget::eventFilter(obj, event);

    // QDateTimeEdit 点击弹出日历（箭头被隐藏，需要手动触发）
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            QDateTimeEdit* dt = nullptr;
            if (w == dateFrom_) dt = dateFrom_;
            else if (w == dateTo_) dt = dateTo_;
            else {
                // w 可能是 dateFrom_/dateTo_ 内部的子控件，沿父链找到 QDateTimeEdit
                for (QWidget* p = w->parentWidget(); p; p = p->parentWidget()) {
                    if (p == dateFrom_) { dt = dateFrom_; break; }
                    if (p == dateTo_) { dt = dateTo_; break; }
                }
            }
            if (dt) {
                auto* cal = new QCalendarWidget;
                cal->setWindowFlags(Qt::Popup);
                cal->setAttribute(Qt::WA_DeleteOnClose);
                cal->setCurrentPage(dt->date().year(), dt->date().month());
                cal->setNavigationBarVisible(true);
                cal->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
                cal->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
                cal->setFirstDayOfWeek(Qt::Monday);
                cal->setGridVisible(false);
                cal->setFixedSize(280, 260);
                cal->setStyleSheet(
                    "QCalendarWidget {"
                    "  background: #FFFFFF;"
                    "  border: 1px solid #E5E7EB;"
                    "  border-radius: 8px;"
                    "}"
                    "QCalendarWidget QToolButton {"
                    "  background: transparent;"
                    "  color: #4B5563;"
                    "  font-size: 13px;"
                    "  padding: 4px 8px;"
                    "  border: none;"
                    "  border-radius: 4px;"
                    "  margin: 2px;"
                    "}"
                    "QCalendarWidget QToolButton:hover {"
                    "  background: #F3F4F6;"
                    "}"
                    "QCalendarWidget QToolButton::menu-indicator {"
                    "  image: none;"
                    "  width: 0; height: 0;"
                    "}"
                    "QCalendarWidget QSpinBox {"
                    "  background: transparent;"
                    "  border: none;"
                    "  color: #1F2937;"
                    "  font-size: 13px;"
                    "  font-weight: 500;"
                    "  padding: 2px 4px;"
                    "}"
                    "QCalendarWidget QSpinBox::up-button,"
                    "QCalendarWidget QSpinBox::down-button {"
                    "  width: 0; height: 0;"
                    "}"
                    "QCalendarWidget QWidget#qt_calendar_navigationbar {"
                    "  background: #FFFFFF;"
                    "  border: none;"
                    "  padding: 6px 8px 4px 8px;"
                    "  spacing: 4px;"
                    "}"
                    "QCalendarWidget QAbstractItemView:enabled {"
                    "  color: #1F2937;"
                    "  font-size: 12px;"
                    "  selection-background-color: #3B82F6;"
                    "  selection-color: #FFFFFF;"
                    "  background: #FFFFFF;"
                    "  outline: none;"
                    "  border: none;"
                    "}"
                    "QCalendarWidget QAbstractItemView:disabled {"
                    "  color: #D1D5DB;"
                    "}"
                    "QCalendarWidget QAbstractItemView::item {"
                    "  padding: 6px;"
                    "  margin: 1px;"
                    "  border-radius: 50%;"
                    "}"
                    "QCalendarWidget QAbstractItemView::item:hover {"
                    "  background: #F3F4F6;"
                    "}"
                    "QCalendarWidget QAbstractItemView::item:selected {"
                    "  background: #3B82F6;"
                    "  color: #FFFFFF;"
                    "}"
                    "QCalendarWidget QTableView {"
                    "  border: none;"
                    "  gridline-color: transparent;"
                    "}"
                    "QCalendarWidget QTableView QTableCornerButton::section {"
                    "  background: #FFFFFF;"
                    "  border: none;"
                    "}");
                cal->move(dt->mapToGlobal(QPoint(0, dt->height())));
                cal->show();
                // 替换导航栏箭头按钮文字为符号
                QMetaObject::invokeMethod(cal, [this, cal, dt]() {
                    auto* nav = cal->findChild<QWidget*>("qt_calendar_navigationbar");
                    if (!nav) return;
                    auto btns = nav->findChildren<QToolButton*>();
                    // 默认顺序：prevMonth(0), prevYear(1,隐藏), nextYear(2,隐藏), nextMonth(3)
                    if (btns.size() >= 4) {
                        btns[0]->setArrowType(Qt::NoArrow);
                        btns[0]->setText("◁");
                        btns[0]->setToolTip(tr("Previous month"));
                        btns[0]->show();
                        btns[1]->setArrowType(Qt::NoArrow);
                        btns[1]->setText("◀");
                        btns[1]->setToolTip(tr("Previous year"));
                        btns[1]->show();
                        btns[2]->setArrowType(Qt::NoArrow);
                        btns[2]->setText("▶");
                        btns[2]->setToolTip(tr("Next year"));
                        btns[2]->show();
                        btns[3]->setArrowType(Qt::NoArrow);
                        btns[3]->setText("▷");
                        btns[3]->setToolTip(tr("Next month"));
                        btns[3]->show();
                    }
                    // 样式
                    for (auto* b : btns) {
                        b->setFixedSize(24, 24);
                        b->setStyleSheet(
                            "QToolButton {"
                            "  background: transparent;"
                            "  color: #6B7280;"
                            "  font-size: 16px;"
                            "  border: none;"
                            "  border-radius: 4px;"
                            "  padding: 0px;"
                            "}"
                            "QToolButton:hover {"
                            "  background: #F3F4F6;"
                            "  color: #0B7C72;"
                            "}");
                    }
                }, Qt::QueuedConnection);
                connect(cal, &QCalendarWidget::clicked, this, [dt](const QDate& d) {
                    dt->setDate(d);
                });
                return true;
            }
        }
    }

    // 找到带有 _kb_rowClick 的行（自身或沿父链向上）
    int docId = 0;
    QWidget* rowWidget = nullptr;
    if (w->property("_kb_rowClick").isValid()) {
        docId = w->property("_kb_rowClick").toInt();
        rowWidget = w;
    } else {
        for (QWidget* p = w->parentWidget(); p; p = p->parentWidget()) {
            if (p->property("_kb_rowClick").isValid()) {
                docId = p->property("_kb_rowClick").toInt();
                rowWidget = p;
                break;
            }
        }
    }
    if (docId <= 0 || !rowWidget) return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (auto* cb = rowWidget->findChild<QCheckBox*>()) {
                cb->setChecked(!cb->isChecked());
            }
        }
        return true;
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
    emit busyChanged(true);  // \u9a71\u52a8\u4e3b\u7a97\u53e3\u5e95\u90e8\u72b6\u6001\u680f\u6c99\u6f0f
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
        emit busyChanged(false);  // 结束沙漏
        ToastNotification::show(this, QStringLiteral("%1 %2").arg(importCount_).arg(tr("documents imported")), 4000, QColor(11, 124, 114));
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
        // 应用预设标签：在「标签筛选」框中选中了某标签时，新导入的文件自动带上该标签
        int presetTagId = (tagFilterCombo_ && tagFilterCombo_->currentData().toInt() > 0)
                              ? tagFilterCombo_->currentData().toInt()
                              : -1;
        if (presetTagId > 0) {
            km.setDocumentTags(newId, QList<int>{presetTagId});
        }
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
void KnowledgeBasePage::onTagFilterChanged(int) { refreshList(); }
void KnowledgeBasePage::onDateFilterChanged() { refreshList(); }
void KnowledgeBasePage::onAddNewTag() {
    // 标签管理弹窗（无边框圆角）：可新建标签，也可删除已有标签
    RoundedDlg rd(this);
    auto* dlg = rd.dlg;
    auto* lay = rd.lay;

    auto* title = new QLabel(tr("Manage Tags"));
    title->setStyleSheet("font-size: 15px; font-weight: 600; color: #1C1C1E; background: transparent;");
    lay->addWidget(title);

    auto* hint = new QLabel(tr("Create or remove tags."));
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size: 12px; color: #9CA3AF; background: transparent;");
    lay->addWidget(hint);

    // 新建标签输入行
    auto* inputRow = new QHBoxLayout;
    inputRow->setSpacing(6);
    auto* nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(tr("New tag name…"));
    nameEdit->setFixedHeight(30);
    nameEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #DDE1E5; border-radius: 8px; padding: 0 10px;"
        " font-size: 12px; background: #F8FAFA; }"
        "QLineEdit:focus { border-color: #0B7C72; background: #FFFFFF; }");
    inputRow->addWidget(nameEdit, 1);
    auto* addBtn = new QPushButton(tr("Add"));
    addBtn->setObjectName("primaryBtn");
    addBtn->setFixedHeight(30);
    inputRow->addWidget(addBtn);
    lay->addLayout(inputRow);

    auto addTagName = [this, nameEdit]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) return;
        auto& km = KnowledgeBaseManager::getInstance();
        if (km.addTag(name)) {
            nameEdit->clear();
            refreshTags();
        } else {
            emit statusMessage(tr("Tag already exists: %1").arg(name));
        }
    };
    connect(addBtn, &QPushButton::clicked, dlg, addTagName);
    connect(nameEdit, &QLineEdit::returnPressed, dlg, addTagName);

    // 已有标签列表（可删除）
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setMaximumHeight(220);
    scroll->setStyleSheet("QScrollArea { border: 1px solid #E8ECEF; border-radius: 8px; background: transparent; }");
    auto* listHost = new QWidget;
    listHost->setStyleSheet("background: transparent;");
    auto* listLay = new QVBoxLayout(listHost);
    listLay->setContentsMargins(8, 8, 8, 8);
    listLay->setSpacing(4);
    scroll->setWidget(listHost);
    lay->addWidget(scroll, 1);

    std::function<void()> rebuildTagList = [this, listLay, dlg, &rebuildTagList]() {
        // 清空
        while (auto* it = listLay->takeAt(0)) {
            if (auto* w = it->widget()) w->deleteLater();
            delete it;
        }
        auto tags = KnowledgeBaseManager::getInstance().getAllTags();
        if (tags.isEmpty()) {
            auto* none = new QLabel(tr("No tags yet."));
            none->setStyleSheet("color: #9CA3AF; font-size: 12px; background: transparent; padding: 12px;");
            none->setAlignment(Qt::AlignCenter);
            listLay->addWidget(none);
        } else {
            for (const auto& t : tags) {
                auto* row = new QWidget;
                row->setStyleSheet("background: transparent;");
                auto* rh = new QHBoxLayout(row);
                rh->setContentsMargins(4, 2, 4, 2);
                rh->setSpacing(6);
                auto* badge = new QLabel(t.second);
                badge->setStyleSheet(
                    "QLabel { background: #F0F7F6; color: #0B7C72; border-radius: 4px;"
                    " padding: 2px 10px; font-size: 12px; }");
                rh->addWidget(badge, 1);
                auto* delBtn = new QPushButton(tr("Delete"));
                delBtn->setFixedHeight(26);
                delBtn->setCursor(Qt::PointingHandCursor);
                delBtn->setStyleSheet(
                    "QPushButton { border: 1px solid #FECACA; border-radius: 6px; padding: 0 10px;"
                    " background: transparent; color: #DC2626; font-size: 11px; }"
                    "QPushButton:hover { background: #FEF2F2; }");
                connect(delBtn, &QPushButton::clicked, dlg, [this, t, rebuildTagList]() {
                    auto& km = KnowledgeBaseManager::getInstance();
                    km.deleteTag(t.first);
                    refreshTags();
                    refreshList();
                    rebuildTagList();
                });
                rh->addWidget(delBtn);
                listLay->addWidget(row);
            }
        }
        listLay->addStretch();
    };
    rebuildTagList();

    // 底部按钮行（无 Close 按钮，用「完成」）
    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 4, 0, 0);
    btnRow->setSpacing(8);
    btnRow->addStretch();
    auto* doneBtn = new QPushButton(tr("Done"));
    doneBtn->setObjectName("primaryBtn");
    doneBtn->setFixedHeight(30);
    doneBtn->setCursor(Qt::PointingHandCursor);
    connect(doneBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    btnRow->addWidget(doneBtn);
    lay->addLayout(btnRow);

    rd.applyDialogSize(340);
    dlg->exec();
    refreshTags();
    refreshList();
}
void KnowledgeBasePage::refreshTags() {
    if (tagFilterCombo_) {
        tagFilterCombo_->blockSignals(true);
        tagFilterCombo_->clear();
        tagFilterCombo_->addItem(tr("All Documents"), -1);
        tagFilterCombo_->addItem(tr("No tag"), -2);
        for (const auto& t : KnowledgeBaseManager::getInstance().getAllTags())
            tagFilterCombo_->addItem(t.second, t.first);
        tagFilterCombo_->blockSignals(false);
    }
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

    // 批量删除确认弹窗（无边框圆角，风格与添加标签一致）
    RoundedDlg rd(this);
    auto* dlg = rd.dlg;
    auto* lay = rd.lay;

    auto* title = new QLabel(tr("Batch Delete"));
    title->setStyleSheet("font-size: 15px; font-weight: 600; color: #1C1C1E; background: transparent;");
    lay->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("%1 %2？").arg(checkedDocIds_.size()).arg(tr("documents to delete")));
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size: 12px; color: #374151; background: transparent;");
    lay->addWidget(hint);

    lay->addSpacing(6);

    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 4, 0, 0);
    btnRow->setSpacing(8);
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setObjectName("secondaryBtn");
    cancelBtn->setFixedHeight(30);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    btnRow->addWidget(cancelBtn);
    auto* delBtn = new QPushButton(tr("Delete"));
    delBtn->setObjectName("primaryBtn");
    delBtn->setFixedHeight(30);
    delBtn->setCursor(Qt::PointingHandCursor);
    connect(delBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    btnRow->addWidget(delBtn);
    lay->addLayout(btnRow);

    rd.applyDialogSize(300);

    if (dlg->exec() != QDialog::Accepted) return;
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
        QMessageBox::information(this, tr("Batch Tag"),
                                 tr("No tags yet. Create one first with the +Tag button."));
        return;
    }

    RoundedDlg rd(this);
    auto* dlg = rd.dlg;
    auto* lay = rd.lay;

    auto* title = new QLabel(tr("Batch Modify Tags"));
    title->setStyleSheet("font-size: 15px; font-weight: 600; color: #1C1C1E; background: transparent;");
    lay->addWidget(title);

    lay->addSpacing(2);

    // 标签列表放在滚动区：过多时滚动，普通时自适应高度
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setMaximumHeight(260);
    scroll->setStyleSheet(
        "QScrollArea { border: 1px solid #E8ECEF; border-radius: 8px; background: transparent; }");
    auto* listHost = new QWidget;
    listHost->setStyleSheet("background: transparent;");
    auto* listLay = new QVBoxLayout(listHost);
    listLay->setContentsMargins(8, 8, 8, 8);
    listLay->setSpacing(6);
    QList<QCheckBox*> checks;
    for (const auto& t : allTags) {
        auto* cb = new QCheckBox(t.second);
        cb->setStyleSheet("font-size: 12px; color: #1C1C1E; background: transparent;");
        checks.append(cb);
        listLay->addWidget(cb);
    }
    scroll->setWidget(listHost);
    lay->addWidget(scroll, 1);

    // 底部按钮（紧凑）
    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 4, 0, 0);
    btnRow->setSpacing(8);
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setObjectName("secondaryBtn");
    cancelBtn->setFixedHeight(30);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    btnRow->addWidget(cancelBtn);
    auto* okBtn = new QPushButton(tr("OK"));
    okBtn->setObjectName("primaryBtn");
    okBtn->setFixedHeight(30);
    okBtn->setCursor(Qt::PointingHandCursor);
    connect(okBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    rd.applyDialogSize(320);

    if (dlg->exec() == QDialog::Accepted) {
        QList<int> selected;
        for (int i = 0; i < checks.size(); ++i)
            if (checks[i]->isChecked()) selected.append(allTags[i].first);
        for (int id : checkedDocIds_)
            km.setDocumentTags(id, selected);
        emit statusMessage(QStringLiteral("%1 %2").arg(checkedDocIds_.size()).arg(tr("documents tagged")));
        refreshList();
    }
}

// ============================================================
// onBatchExport — 把选中的文档原始文件复制到选择的文件夹
// ============================================================
void KnowledgeBasePage::onBatchExport() {
    if (checkedDocIds_.isEmpty()) return;

    // 自定义目标目录框：可直接输入/粘贴路径，或 Browse 打开目录框点选/新建。
    // 明确由 OK 按钮确认，避免 Qt 目录框「回车即确认导出」的问题。
    RoundedDlg rd(this);
    auto* dlg = rd.dlg;
    auto* lay = rd.lay;

    auto* title = new QLabel(tr("Choose export folder"));
    title->setStyleSheet("font-size: 15px; font-weight: 600; color: #1C1C1E; background: transparent;");
    lay->addWidget(title);

    auto* hint = new QLabel(tr("Enter target folder path (auto-created if not exists), or click Browse… to select."));
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size: 12px; color: #9CA3AF; background: transparent;");
    lay->addWidget(hint);

    auto* pathRow = new QHBoxLayout;
    pathRow->setSpacing(6);
    auto* pathEdit = new QLineEdit;
    pathEdit->setPlaceholderText(QStringLiteral("例如 D:\\export"));
    pathEdit->setFixedHeight(32);
    pathEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #DDE1E5; border-radius: 8px; padding: 0 10px;"
        " font-size: 13px; background: #F8FAFA; }"
        "QLineEdit:focus { border-color: #0B7C72; background: #FFFFFF; }");
    pathRow->addWidget(pathEdit, 1);

    auto* browseBtn = new QPushButton(tr("浏览…"));
    browseBtn->setObjectName("secondaryBtn");
    browseBtn->setFixedHeight(32);
    browseBtn->setCursor(Qt::PointingHandCursor);
    connect(browseBtn, &QPushButton::clicked, dlg, [pathEdit]() {
        QFileDialog fd;
        fd.setFileMode(QFileDialog::Directory);
        fd.setOption(QFileDialog::ShowDirsOnly, true);
        fd.setOption(QFileDialog::DontUseNativeDialog, true);
        // 若输入框里已有路径，让目录框从这里进入（不存在才用默认）
        QString start = pathEdit->text().trimmed();
        if (!start.isEmpty() && QDir(start).exists())
            fd.setDirectory(start);
        if (fd.exec() == QDialog::Accepted) {
            QString d = fd.selectedFiles().isEmpty() ? fd.directory().absolutePath()
                                                     : fd.selectedFiles().first();
            if (!d.isEmpty()) pathEdit->setText(d);
        }
    });
    pathRow->addWidget(browseBtn);
    lay->addLayout(pathRow);

    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 4, 0, 0);
    btnRow->setSpacing(8);
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setObjectName("secondaryBtn");
    cancelBtn->setFixedHeight(30);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    btnRow->addWidget(cancelBtn);
    auto* okBtn = new QPushButton(tr("OK"));
    okBtn->setObjectName("primaryBtn");
    okBtn->setFixedHeight(30);
    okBtn->setCursor(Qt::PointingHandCursor);
    connect(okBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    rd.applyDialogSize(360);

    if (dlg->exec() != QDialog::Accepted) return;
    QString dir = pathEdit->text().trimmed();
    if (dir.isEmpty()) return;
    // 若目录不存在则创建
    QDir().mkpath(dir);

    auto& km = KnowledgeBaseManager::getInstance();
    int okCount = 0, skipCount = 0;
    QStringList skippedNames;
    for (int id : checkedDocIds_) {
        auto e = km.getEntry(id);
        QString src = e.sourcePath;
        if (src.isEmpty()) { skipCount++; skippedNames << (e.title.isEmpty() ? QString::number(id) : e.title); continue; }
        QFileInfo si(src);
        if (!si.exists() || !si.isFile()) { skipCount++; skippedNames << si.fileName(); continue; }

        // 源文件旁可能带 assets/ 图片文件夹（markdown 以 assets/xxx 引用）。
        // 只有当本文档的 markdown 确实引用了 assets/ 图片时才需要导出该文件夹，
        // 避免把同目录下其它文件生成的 assets/ 一并误带过去。
        QString assetsDir = si.absolutePath() + "/assets";
        bool markdownRefsAssets = e.markdownContent.contains("assets/", Qt::CaseInsensitive);
        bool hasAssets = markdownRefsAssets && QDir(assetsDir).exists();

        QString destDir = dir;
        if (hasAssets) {
            // 带图片的导出到一个独立子文件夹，避免不同文档的 assets 冲突
            QString base = si.completeBaseName();
            destDir = dir + "/" + base;
            int n = 2;
            while (QDir(destDir).exists())
                destDir = dir + "/" + base + QStringLiteral("(%1)").arg(n++);
            QDir().mkpath(destDir);
        }

        QString dest = destDir + "/" + si.fileName();
        if (QFile::exists(dest)) QFile::remove(dest);
        bool fileOk = QFile::copy(src, dest);

        // 导出源文件即算成功；assets 图片尽力复制，失败不影响导出
        if (fileOk && hasAssets)
            copyDirectoryRecursive(assetsDir, destDir + "/assets");

        if (fileOk) okCount++;
        else { skipCount++; skippedNames << si.fileName(); }
    }

    emit statusMessage(QStringLiteral("%1 %2").arg(okCount).arg(tr("documents exported")));
    if (skipCount > 0) {
        emit statusMessage(QStringLiteral("%1 %2：%3")
            .arg(skipCount).arg(tr("failed to export")).arg(skippedNames.join(QStringLiteral("、"))));
    } else {
        ToastNotification::show(this, QStringLiteral("已导出 %1 个文件").arg(okCount), 4000,
                                QColor(11, 124, 114), QString(),
                                tr("Open this folder"), QUrl::fromLocalFile(dir).toString());
    }
}

