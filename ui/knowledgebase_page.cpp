#include "knowledgebase_page.h"
#include "knowledgebase_manager.h"
#include "mainwindow.h"        // DropZoneWidget
#include <QFrame>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDateTime>
#include <QDialog>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QApplication>
#include <QFileInfo>
#include <QTimer>
#include <QRegularExpression>
#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QDir>

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

    // 获取堆栈回溯
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
    return EXCEPTION_CONTINUE_SEARCH;  // 让系统继续默认处理（闪退）
}

// 在库加载时安装 VEH
namespace {
    struct InstallVEH {
        InstallVEH() { AddVectoredExceptionHandler(1, crashHandler); }
    } _vehInstaller;
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
// setupUI — 上传区 → 工具栏 → 文档列表 → 底部批量操作栏
// ============================================================
void KnowledgeBasePage::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // ---- 上传横条 ----
    dropZone_ = new DropZoneWidget;
    dropZone_->setFixedHeight(64);
    dropZone_->setStyleSheet(QStringLiteral(
        "DropZoneWidget { background: #F0F7F6; border: 2px dashed #D0E8E4; border-radius: 10px; }"
        "DropZoneWidget:hover { background: #E8F5F3; border-color: #0B7C72; }"));
    auto* dropLayout = new QHBoxLayout(dropZone_);
    dropLayout->setContentsMargins(20, 0, 20, 0);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(8);
    auto* dropIcon = new QLabel(QStringLiteral("\U0001F4C1"));
    dropIcon->setStyleSheet("font-size: 20px; background: transparent;");
    dropLayout->addWidget(dropIcon);
    auto* dropText = new QLabel("拖拽文件到此处归档 · 支持 PDF / DOCX / XLSX / PPTX / MD / TXT / 图片");
    dropText->setStyleSheet("font-size: 12px; font-weight: 500; color: #6B7280; background: transparent;");
    dropLayout->addWidget(dropText);
    dropLayout->addStretch();
    connect(dropZone_, &DropZoneWidget::fileDropped, this, &KnowledgeBasePage::onFileDropped);
    layout->addWidget(dropZone_);

    // ---- 工具栏 ----
    auto* toolbar = new QFrame;
    toolbar->setObjectName("card");
    toolbar->setStyleSheet(QStringLiteral("QFrame#card { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 10px; }"));
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);
    toolbarLayout->setSpacing(8);

    searchInput_ = new QLineEdit;
    searchInput_->setPlaceholderText("搜索标题、全文内容…");
    searchInput_->setFixedHeight(30);
    searchInput_->setFixedWidth(160);
    connect(searchInput_, &QLineEdit::textChanged, this, &KnowledgeBasePage::onSearchTextChanged);
    toolbarLayout->addWidget(searchInput_, 0);

    dateFrom_ = new QDateEdit;
    dateFrom_->setDisplayFormat("yyyy-MM-dd");
    dateFrom_->setCalendarPopup(true);
    dateFrom_->setFixedHeight(30);
    dateFrom_->setFixedWidth(130);
    dateFrom_->setSpecialValueText("起始日期");
    dateFrom_->setDate(QDate::currentDate().addMonths(-3));
    connect(dateFrom_, &QDateEdit::dateChanged, this, &KnowledgeBasePage::onDateFilterChanged);
    toolbarLayout->addWidget(dateFrom_, 0);

    auto* dateSep = new QLabel("~");
    dateSep->setStyleSheet("color: #9CA3AF; background: transparent;");
    toolbarLayout->addWidget(dateSep, 0);

    dateTo_ = new QDateEdit;
    dateTo_->setDisplayFormat("yyyy-MM-dd");
    dateTo_->setCalendarPopup(true);
    dateTo_->setFixedHeight(30);
    dateTo_->setFixedWidth(130);
    dateTo_->setSpecialValueText("截止日期");
    dateTo_->setDate(QDate::currentDate());
    connect(dateTo_, &QDateEdit::dateChanged, this, &KnowledgeBasePage::onDateFilterChanged);
    toolbarLayout->addWidget(dateTo_, 0);

    auto* clearDateBtn = new QPushButton("清除日期");
    clearDateBtn->setFixedHeight(30);
    clearDateBtn->setStyleSheet(
        "QPushButton { border: 1px solid #D1D5DB; border-radius: 6px; padding: 0 10px; background: transparent; color: #6B7280; font-size: 11px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(clearDateBtn, &QPushButton::clicked, this, [this]() {
        dateFrom_->clear();
        dateTo_->clear();
        refreshList();
    });
    toolbarLayout->addWidget(clearDateBtn, 0);

    tagFilterCombo_ = new QComboBox;
    tagFilterCombo_->setFixedHeight(30);
    tagFilterCombo_->setMinimumWidth(110);
    connect(tagFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KnowledgeBasePage::onTagFilterChanged);
    toolbarLayout->addWidget(tagFilterCombo_);

    auto* addTagBtn = new QPushButton("＋新建标签");
    addTagBtn->setFixedHeight(30);
    addTagBtn->setStyleSheet(
        "QPushButton { border: 1px solid #D1D5DB; border-radius: 6px; padding: 0 12px; background: transparent; color: #374151; font-size: 12px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(addTagBtn, &QPushButton::clicked, this, &KnowledgeBasePage::onAddNewTag);
    toolbarLayout->addWidget(addTagBtn);

    auto* batchBtn = new QPushButton("批量导入");
    batchBtn->setObjectName("primaryBtn");
    batchBtn->setFixedHeight(30);
    connect(batchBtn, &QPushButton::clicked, this, &KnowledgeBasePage::onBatchImport);
    toolbarLayout->addWidget(batchBtn);

    selectAllBtn_ = new QPushButton("全选");
    selectAllBtn_->setFixedHeight(30);
    selectAllBtn_->setStyleSheet(
        "QPushButton { border: 1px solid #D1D5DB; border-radius: 6px; padding: 0 12px; background: transparent; color: #374151; font-size: 12px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(selectAllBtn_, &QPushButton::clicked, this, &KnowledgeBasePage::onSelectAll);
    toolbarLayout->addWidget(selectAllBtn_);
    layout->addWidget(toolbar);

    // ---- 文档列表（占满剩余高度） ----
    listScroll_ = new QScrollArea;
    listScroll_->setWidgetResizable(true);
    listScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    listScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listScroll_->setMinimumHeight(100);
    listScroll_->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 6px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #D0D4D8; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    listContainer_ = new QWidget;
    listContainer_->setStyleSheet("QWidget { background: transparent; }");
    listLayout_ = new QVBoxLayout(listContainer_);
    listLayout_->setContentsMargins(0, 0, 0, 0);
    listLayout_->setSpacing(4);

    emptyHint_ = new QLabel("暂无文档\n拖拽文件到上方上传区归档");
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setStyleSheet("color: #9CA3AF; font-size: 13px; padding: 40px; background: transparent; border: none;");
    emptyHint_->setWordWrap(true);
    listLayout_->addWidget(emptyHint_);
    listLayout_->addStretch();

    listScroll_->setWidget(listContainer_);
    layout->addWidget(listScroll_, 1);

    // ---- 底部批量操作栏（初始隐藏） ----
    batchBar_ = new QFrame;
    batchBar_->setObjectName("card");
    batchBar_->setStyleSheet(QStringLiteral(
        "QFrame#card { background: #F0F7F6; border: 1px solid #D0E8E4; border-radius: 10px; }"));
    auto* batchLayout = new QHBoxLayout(batchBar_);
    batchLayout->setContentsMargins(14, 8, 14, 8);
    batchLayout->setSpacing(10);

    batchCountLabel_ = new QLabel("已选择 0 项");
    batchCountLabel_->setStyleSheet("font-size: 12px; color: #374151; background: transparent; border: none;");
    batchLayout->addWidget(batchCountLabel_);
    batchLayout->addStretch();

    batchTagBtn_ = new QPushButton("批量修改标签");
    batchTagBtn_->setStyleSheet(
        "QPushButton { border: 1px solid #D1D5DB; border-radius: 6px; padding: 0 14px; background: transparent; color: #374151; font-size: 12px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(batchTagBtn_, &QPushButton::clicked, this, &KnowledgeBasePage::onBatchChangeTags);
    batchLayout->addWidget(batchTagBtn_);

    batchDelBtn_ = new QPushButton("批量删除");
    batchDelBtn_->setStyleSheet(
        "QPushButton { border: 1px solid #EF4444; border-radius: 6px; padding: 0 14px; background: transparent; color: #EF4444; font-size: 12px; }"
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
    // 倒序清理旧 widget：只保留 emptyHint_ 和 Stretch，其余全部删除
    for (int i = listLayout_->count() - 1; i >= 0; --i) {
        QLayoutItem* item = listLayout_->itemAt(i);
        if (!item) continue;

        // 跳过 emptyHint_ 和 Stretch（按指针精确识别）
        if (item->widget() == emptyHint_) continue;
        if (item->spacerItem()) continue;

        // 删除文件项 widget
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
    if (selectAllBtn_) selectAllBtn_->setText("全选");
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

    emptyHint_->setVisible(docs.isEmpty());
    for (const auto& doc : docs) {
        auto tags = km.getDocumentTagNames(doc.id);
        QString summary = doc.summary.isEmpty() ? "(暂无摘要)" : doc.summary;
        auto* item = createListItem(doc.id, doc.title,
            doc.createdAt.toString("yyyy-MM-dd HH:mm"), doc.fileType.toUpper(), tags, summary, keyword);
        listLayout_->insertWidget(listLayout_->count() - 1, item);
    }
}

// ============================================================
// createListItem — 对齐文件翻译页面样式，带多选 CheckBox
// ============================================================
QWidget* KnowledgeBasePage::createListItem(int id, const QString& title,
    const QString& date, const QString& fileType,
    const QStringList& tags, const QString& summary, const QString& keyword) {
    auto* item = new QFrame;
    item->setProperty("docId", id);
    item->setStyleSheet(
        "QFrame { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 8px; }"
        "QFrame:hover { background: rgba(11, 124, 114, 0.06); }");

    auto* mainLayout = new QVBoxLayout(item);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(0);

    // ---- 标题行 ----
    auto* headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);

    // 多选 CheckBox
    auto* checkBox = new QCheckBox;
    checkBox->setProperty("docId", id);
    checkBox->setStyleSheet(
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 1.5px solid #D0D4D8; background: #FFFFFF; }"
        "QCheckBox::indicator:hover { border-color: #0B7C72; }"
        "QCheckBox::indicator:checked { background-color: #0B7C72; border-color: #0B7C72; }");
    connect(checkBox, &QCheckBox::toggled, this, [this, id, checkBox](bool checked) {
        if (checked) checkedDocIds_.insert(id);
        else checkedDocIds_.remove(id);
        updateBatchBar();
    });
    headerLayout->addWidget(checkBox);

    // 高亮辅助函数
    auto highlightText = [](const QString& text, const QString& kw) -> QString {
        if (kw.isEmpty()) return text.toHtmlEscaped();
        QString escaped = text.toHtmlEscaped();
        QRegularExpression re("(" + QRegularExpression::escape(kw) + ")", QRegularExpression::CaseInsensitiveOption);
        return escaped.replace(re, "<span style='background:#FFF3CD;color:#856404;border-radius:2px;padding:0 1px;'>\\1</span>");
    };

    // 文件类型图标
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
        iconLbl->setPixmap(pix.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLbl->setStyleSheet("background: transparent; border: none;");
    headerLayout->addWidget(iconLbl);

    // 标题 + 元数据
    auto* nameLayout = new QVBoxLayout;
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->setSpacing(2);
    auto* titleLbl = new QLabel(highlightText(title, keyword));
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setStyleSheet("font-weight: 500; font-size: 13px; color: #1C1C1E; background: transparent; border: none;");
    nameLayout->addWidget(titleLbl);

    QString tagStr = tags.isEmpty() ? "未分类" : tags.join(", ");
    auto* metaLbl = new QLabel(
        QStringLiteral("<span style='color:#8E8E93;font-size:11px;'>%1 · %2</span>"
            " <span style='background:#E8F5F3;color:#0B7C72;border-radius:8px;padding:0 6px;font-size:10px;'>%3</span>")
            .arg(date, fileType, tagStr));
    metaLbl->setTextFormat(Qt::RichText);
    metaLbl->setStyleSheet("background: transparent; border: none;");
    nameLayout->addWidget(metaLbl);
    headerLayout->addLayout(nameLayout, 1);

    // 展开箭头
    auto* arrowLabel = new QLabel("▶");
    arrowLabel->setStyleSheet("font-size: 10px; color: #9CA3AF; background: transparent; border: none;");
    headerLayout->addWidget(arrowLabel);

    mainLayout->addLayout(headerLayout);

    // ---- 详情区域（折叠） ----
    auto* detailWidget = new QWidget;
    detailWidget->setVisible(false);
    detailWidget->setStyleSheet("background: transparent; border: none;");
    auto* detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(52, 6, 0, 0);
    detailLayout->setSpacing(6);

    auto* sumLbl = new QLabel(summary);
    sumLbl->setWordWrap(true);
    sumLbl->setStyleSheet("font-size: 12px; color: #5B6269; line-height: 1.5; background: transparent; border: none;");
    detailLayout->addWidget(sumLbl);
    mainLayout->addWidget(detailWidget);

    // ---- 箭头点击展开 ----
    arrowLabel->installEventFilter(this);
    arrowLabel->setProperty("_kb_detailWidget", QVariant::fromValue(reinterpret_cast<quintptr>(detailWidget)));
    arrowLabel->setProperty("_kb_arrowLabel", QVariant::fromValue(reinterpret_cast<quintptr>(arrowLabel)));

    // ---- 双击 item 打开详情 ----
    item->installEventFilter(this);
    item->setProperty("_kb_docId", id);
    item->setProperty("_kb_doubleClick", true);

    return item;
}

// ============================================================
// eventFilter — 箭头点击展开/折叠 + 双击查看详情
// ============================================================
bool KnowledgeBasePage::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (!w) return QWidget::eventFilter(obj, event);
        if (w->property("_kb_detailWidget").isValid()) {
            auto* detailW = reinterpret_cast<QWidget*>(w->property("_kb_detailWidget").value<quintptr>());
            auto* arrowLbl = reinterpret_cast<QLabel*>(w->property("_kb_arrowLabel").value<quintptr>());
            if (detailW && arrowLbl) {
                bool vis = detailW->isVisible();
                detailW->setVisible(!vis);
                arrowLbl->setText(vis ? "▶" : "▼");
                return true;
            }
        }
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (!w) return QWidget::eventFilter(obj, event);
        if (w->property("_kb_doubleClick").toBool()) {
            int docId = w->property("_kb_docId").toInt();
            if (docId > 0) {
                showDocumentDetail(docId);
                return true;
            }
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
    if (filePath.isEmpty()) {
        emit statusMessage("源文件路径为空");
        return;
    }
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        emit statusMessage(QStringLiteral("源文件不存在: %1").arg(filePath));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

// ============================================================
// ============================================================
void KnowledgeBasePage::onFileDropped(const QStringList& paths) {
    if (isImporting_) {
        emit statusMessage("正在处理中，请等待完成后再上传");
        return;
    }
    static bool processing = false;
    if (processing) return;
    processing = true;

    QStringList files = paths;
    if (files.isEmpty()) {
        files = QFileDialog::getOpenFileNames(this, "选择文件", QString(),
            "所有支持的文件 (*.pdf *.docx *.xlsx *.pptx *.md *.txt *.png *.jpg *.jpeg *.bmp *.tiff);;所有文件 (*)");
    }
    if (files.isEmpty()) { processing = false; return; }

    // 去重（QFileDialog 在某些情况下可能返回重复路径）
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

    setEnabled(false);
    int total = tasks.size();
    emit statusMessage(QStringLiteral("正在解析 %1 个文件…").arg(total));

    QString baseDir = QCoreApplication::applicationDirPath();

    // 禁用界面，分批处理文件（每次处理一个，防止 UI 假死）
    setEnabled(false);

    // 递增操作代次，使之前的 QTimer 链全部失效
    importGeneration_++;

    // 保存任务列表到成员变量
    pendingTasks_ = tasks;
    pendingBaseDir_ = baseDir;
    processIndex_ = 0;
    importCount_ = 0;
    isImporting_ = true;
    int myGen = importGeneration_;

    // 延时启动处理，让 UI 先刷新（文件对话框关闭事件先处理完）
    QTimer::singleShot(100, this, [this, myGen]() {
        // 如果在这 100ms 内又开始了新的上传，直接返回
        if (myGen != importGeneration_) return;
        processNextFile(myGen);
        processing = false;
    });
}

// ============================================================
// extract_image_text — 直接用 OCR 引擎识别图片（绕过 FileTranslationModule 避免 ReleaseProcessor 崩溃）
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
            if (!r.text.empty()) {
                text += r.text + "\n";
            }
        }
        return text;
    }
    return "";
}

// ============================================================
// extract_pdf_text — 用 PDFium 渲染 PDF 为图片，再用 OCR 识别
// ============================================================
std::string KnowledgeBasePage::extract_pdf_text(const std::string& pdf_path, const std::string& base_dir, int dpi) {
    std::vector<cv::Mat> pages;
    // PDF 渲染（和 FileTranslationModule::pdf_to_images 相同逻辑）
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
                if (!r.text.empty()) {
                    full_text += r.text + "\n";
                }
            }
        }
        return full_text;
    }
    return "";
}

// ============================================================
// processNextFile — 逐文件处理，每次处理完用 QTimer::singleShot(0)
// 调度下一个，让事件循环自然流转（不用 processEvents 泵入事件）
// myGen: 本次操作的代次，用于使旧定时器链失效
// ============================================================
// 调度下一个，让事件循环自然流转（不用 processEvents 泵入事件）
// myGen: 本次操作的代次，用于使旧定时器链失效
// ============================================================
void KnowledgeBasePage::processNextFile(int myGen) {
    // 如果已经有新的上传开始，立即停止（防止僵尸定时器干扰）
    if (myGen != importGeneration_ || !isImporting_) return;

    // 全部处理完毕
    if (processIndex_ >= pendingTasks_.size()) {
        pendingTasks_.clear();
        refreshList();
        setEnabled(true);
        emit statusMessage(QStringLiteral("导入完成，共 %1 个文件").arg(importCount_));
        importCount_ = 0;
        processIndex_ = 0;
        isImporting_ = false;
        return;
    }

    const ImportTask& task = pendingTasks_[processIndex_];
    emit statusMessage(QStringLiteral("正在解析 %1/%2 …").arg(processIndex_ + 1).arg(pendingTasks_.size()));

    // 解析文件内容（md/txt 直接读取，其他通过引擎提取为 markdown）
    QString ext = task.fileType;
    if (ext == "md" || ext == "txt") {
        // md/txt 轻量操作可以在主线程完成
        QString markdown;
        QFile f(task.filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            markdown = QString::fromUtf8(f.readAll());
            f.close();
        }
        processIndex_++;
        finishEntry(task, markdown);
        QTimer::singleShot(0, this, [this, myGen]() {
            processNextFile(myGen);
        });
    } else {
        // 图片/PDF/Excel 等在后台线程处理，避免阻塞 UI
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
            QTimer::singleShot(0, this, [this, myGen]() {
                processNextFile(myGen);
            });
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
                if (!extracted.empty()) {
                    md = QString::fromStdString(extracted);
                }
            } catch (...) {}
            // 回到主线程入库
            QMetaObject::invokeMethod(QCoreApplication::instance(), [onFinish, md]() {
                onFinish(md);
            }, Qt::QueuedConnection);
        }).detach();
    }
}

void KnowledgeBasePage::onBatchImport() { onFileDropped(QStringList()); }

// ============================================================
// generateSummary
// ============================================================
// QString KnowledgeBasePage::generateSummary(const QString& markdown) {
//     // 取前 1000 字符用于翻译（摘要基于翻译结果截取）
//     QString plain = markdown.simplified();
//     if (plain.length() > 1000) plain = plain.left(1000) + "……";
//     if (plain.trimmed().isEmpty()) return QString();
//     try {
//         // 用翻译引擎翻译成中文，取前 200 字作为摘要
//         auto r = translate_text(plain.toStdString(), "Chinese", 512);
//         QString result = QString::fromStdString(r);
//         if (result.length() > 200) result = result.left(200) + "……";
//         return result;
//     } catch (...) { return plain.left(200); }
// }

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
            // 后台线程生成摘要
            QString savedMarkdown = markdown;
            int savedId = newId;
            std::thread([this, savedMarkdown, savedId]() {
                QString summary;
                auto& ctx = docmind::GlobalEngineContext::getInstance();
                if (ctx.ensureSummarizerEngine()) {
                    if (auto* engine = ctx.getSummarizerEngine()) {
                        std::string text_utf8 = savedMarkdown.toUtf8().constData();
                        if (text_utf8.length() > 4000) {
                            text_utf8 = text_utf8.substr(0, 4000);
                        }
                        std::string aiResult = engine->summarize(text_utf8, 4096);
                        if (!aiResult.empty()) {
                            summary = QString::fromUtf8(aiResult.c_str());
                        }
                    }
                }
                if (summary.isEmpty()) {
                    QString plain = savedMarkdown.simplified();
                    if (plain.length() > 2000) plain = plain.left(2000) + "……";
                    summary = plain.left(200);
                    if (plain.length() > 200) summary += "……";
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
// 工具栏
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
    QString name = QInputDialog::getText(this, "新建标签", "请输入标签名称:", QLineEdit::Normal, {}, &ok);
    if (ok && !name.trimmed().isEmpty()) {
        auto& km = KnowledgeBaseManager::getInstance();
        if (km.addTag(name.trimmed())) { refreshTags(); emit statusMessage("已创建标签 " + name.trimmed()); }
        else emit statusMessage("创建标签失败（可能已存在）");
    }
}
void KnowledgeBasePage::refreshTags() {
    if (!tagFilterCombo_) return;
    tagFilterCombo_->blockSignals(true);
    tagFilterCombo_->clear();
    tagFilterCombo_->addItem("全部文档", -1);
    for (const auto& t : KnowledgeBaseManager::getInstance().getAllTags())
        tagFilterCombo_->addItem(t.second, t.first);
    tagFilterCombo_->blockSignals(false);
}

// ============================================================
// 底部批量操作栏
// ============================================================
void KnowledgeBasePage::updateBatchBar() {
    int n = checkedDocIds_.size();
    batchCountLabel_->setText(QStringLiteral("已选择 %1 项").arg(n));
    batchBar_->setVisible(n > 0);
}

void KnowledgeBasePage::onSelectAll() {
    allSelected_ = !allSelected_;
    if (allSelected_) {
        // 收集当前列表中的所有 docId
        auto& km = KnowledgeBaseManager::getInstance();
        QString keyword = searchInput_ ? searchInput_->text().trimmed() : QString();
        int tagFilter = tagFilterCombo_ ? tagFilterCombo_->currentData().toInt() : -1;
        QList<KnowledgeEntry> docs;
        if (!keyword.isEmpty()) docs = km.searchEntries(keyword);
        else if (tagFilter > 0) docs = km.getEntriesByTag(tagFilter);
        else docs = km.getAllEntries();
        for (const auto& doc : docs) {
            checkedDocIds_.insert(doc.id);
        }
        selectAllBtn_->setText("取消全选");
    } else {
        checkedDocIds_.clear();
        selectAllBtn_->setText("全选");
    }
    // 同步所有 list item 的 checkbox
    for (QCheckBox* cb : findChildren<QCheckBox*>()) {
        if (cb->property("docId").isValid())
            cb->setChecked(allSelected_);
    }
    updateBatchBar();
}

void KnowledgeBasePage::onBatchDelete() {
    if (checkedDocIds_.isEmpty()) return;
    auto reply = QMessageBox::question(this, "批量删除",
        QStringLiteral("确定删除选中的 %1 个文档？").arg(checkedDocIds_.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    auto& km = KnowledgeBaseManager::getInstance();
    QSet<int> ids = checkedDocIds_;  // 拷贝一份，避免遍历过程中修改
    int deleted = km.deleteEntries(QList<int>(ids.begin(), ids.end()));
    emit statusMessage(QStringLiteral("已删除 %1 个文档").arg(deleted));
    refreshList();
}

void KnowledgeBasePage::onBatchChangeTags() {
    if (checkedDocIds_.isEmpty()) return;
    auto& km = KnowledgeBaseManager::getInstance();
    auto allTags = km.getAllTags();
    if (allTags.isEmpty()) {
        QMessageBox::information(this, "修改标签", "暂无标签，请先创建标签");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("批量修改标签（%1 个文档）").arg(checkedDocIds_.size()));
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
        emit statusMessage(QStringLiteral("已更新 %1 个文档的标签").arg(checkedDocIds_.size()));
        refreshList();
    }
}
