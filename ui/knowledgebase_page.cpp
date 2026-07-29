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
#include <QApplication>
#include <QFileInfo>
#include <QTimer>
#include <QScrollBar>
#include <QCoreApplication>
#include <QDir>

#include "docmind/DocumentEngine.h"
#include "docmind/core/GlobalEngineContext.hpp"

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
    connect(searchInput_, &QLineEdit::textChanged, this, &KnowledgeBasePage::onSearchTextChanged);
    toolbarLayout->addWidget(searchInput_, 1);

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
    while (listLayout_->count() > 2) {
        auto* item = listLayout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    checkedDocIds_.clear();
    updateBatchBar();

    auto& km = KnowledgeBaseManager::getInstance();
    QList<KnowledgeEntry> docs;
    QString keyword = searchInput_ ? searchInput_->text().trimmed() : QString();
    int tagFilter = tagFilterCombo_ ? tagFilterCombo_->currentData().toInt() : -1;
    if (!keyword.isEmpty()) docs = km.searchEntries(keyword);
    else if (tagFilter > 0) docs = km.getEntriesByTag(tagFilter);
    else docs = km.getAllEntries();

    emptyHint_->setVisible(docs.isEmpty());
    for (const auto& doc : docs) {
        auto tags = km.getDocumentTagNames(doc.id);
        QString summary = doc.summary.isEmpty() ? "(暂无摘要)" : doc.summary;
        auto* item = createListItem(doc.id, doc.title,
            doc.createdAt.toString("yyyy-MM-dd"), doc.fileType.toUpper(), tags, summary);
        listLayout_->insertWidget(listLayout_->count() - 1, item);
    }
}

// ============================================================
// createListItem — 对齐文件翻译页面样式，带多选 CheckBox
// ============================================================
QWidget* KnowledgeBasePage::createListItem(int id, const QString& title,
    const QString& date, const QString& fileType,
    const QStringList& tags, const QString& summary) {
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
    auto* titleLbl = new QLabel(title);
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

    return item;
}

// ============================================================
// eventFilter — 箭头点击展开/折叠
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
    return QWidget::eventFilter(obj, event);
}

// ============================================================
// ============================================================
void KnowledgeBasePage::onFileDropped(const QStringList& paths) {
    QStringList files = paths;
    if (files.isEmpty()) {
        files = QFileDialog::getOpenFileNames(this, "选择文件", QString(),
            "所有支持的文件 (*.pdf *.docx *.xlsx *.pptx *.md *.txt *.png *.jpg *.jpeg *.bmp *.tiff);;所有文件 (*)");
    }
    if (files.isEmpty()) return;

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
    QApplication::processEvents();

    QString baseDir = QCoreApplication::applicationDirPath();

    // 禁用界面，分批处理文件（每次处理一个，防止 UI 假死）
    setEnabled(false);

    // 保存任务列表到成员变量
    pendingTasks_ = tasks;
    pendingBaseDir_ = baseDir;
    processIndex_ = 0;
    importCount_ = 0;
    isImporting_ = true;

    // 延时启动处理，让 UI 先刷新
    QTimer::singleShot(100, this, &KnowledgeBasePage::processNextFile);
}

// ============================================================
// processNextFile — 逐个处理文件（QTimer 驱动，防止 UI 卡死）
// ============================================================
void KnowledgeBasePage::processNextFile() {
    if (!isImporting_ || processIndex_ >= pendingTasks_.size()) {
        // 全部完成或被取消
        refreshList();
        setEnabled(true);
        emit statusMessage(QStringLiteral("导入完成，共 %1 个文件").arg(importCount_));
        pendingTasks_.clear();
        importCount_ = 0;
        processIndex_ = 0;
        isImporting_ = false;
        return;
    }

    const ImportTask& task = pendingTasks_[processIndex_];
    processIndex_++;

    emit statusMessage(QStringLiteral("正在解析 %1/%2 …").arg(processIndex_).arg(pendingTasks_.size()));
    QApplication::processEvents();

    // 引擎解析
    QString markdown;
    bool parseOk = false;
    QString ext = task.fileType;
    if (ext == "md" || ext == "txt") {
        QFile f(task.filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            markdown = QString::fromUtf8(f.readAll());
            f.close();
            parseOk = true;
        }
    } else {
        // 确保引擎已初始化（首次加载模型需要3-5秒，后续复用缓存）
        emit statusMessage(QStringLiteral("正在加载引擎…"));
        QApplication::processEvents();
        docmind::GlobalEngineContext::getInstance().initialize();
        std::string baseDirStr = pendingBaseDir_.toStdString();
        try {
            std::string outPath;
            if (ext == "pdf") {
                outPath = process_pdf(task.filePath.toStdString(), baseDirStr, "English", 0.5f, 200);
            } else {
                outPath = process_file(task.filePath.toStdString(), "", baseDirStr, "English", 0.5f, 200, true, false);
            }
            QFile f(QString::fromStdString(outPath));
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                markdown = QString::fromUtf8(f.readAll());
                f.close();
                parseOk = true;
            }
            QFile::remove(QString::fromStdString(outPath));
        } catch (const std::exception& e) {
            markdown = QStringLiteral("解析失败: %1").arg(e.what());
        } catch (...) {
            markdown = "解析失败: 未知错误";
        }
    }

    // 入库
    auto& km = KnowledgeBaseManager::getInstance();
    KnowledgeEntry entry;
    entry.title = task.title;
    entry.fileType = task.fileType;
    entry.sourcePath = task.filePath;
    entry.fileSize = task.fileSize;
    entry.markdownContent = markdown;

    int newId = -1;
    if (km.addEntry(entry, &newId)) {
        if (parseOk && !markdown.trimmed().isEmpty()) {
            // 生成 AI 摘要（调用 Hy-MT2 模型的 summarize 接口）
            emit statusMessage(QStringLiteral("正在生成摘要 …"));
            QApplication::processEvents();
            QString plain = markdown.simplified();
            if (plain.length() > 2000) plain = plain.left(2000) + "……";
            QString summary;
            try {
                auto& ctx = docmind::GlobalEngineContext::getInstance();
                ctx.ensureTranslatorEngine();
                auto* translator = ctx.getTranslatorEngine();
                if (translator) {
                    auto r = translator->summarize(plain.toStdString(), 256);
                    summary = QString::fromStdString(r);
                }
            } catch (const std::exception& e) {
                summary = plain.left(200);
            }
            if (!summary.isEmpty()) km.updateSummary(newId, summary);
        }
        importCount_++;
    }

    emit statusMessage(QStringLiteral("已处理 %1/%2 个文件（跳过摘要）").arg(importCount_).arg(pendingTasks_.size()));
    QApplication::processEvents();

    // 处理下一个文件
    QTimer::singleShot(10, this, &KnowledgeBasePage::processNextFile);
}

void KnowledgeBasePage::onBatchImport() { onFileDropped(QStringList()); }

// ============================================================
// generateSummary
// ============================================================
QString KnowledgeBasePage::generateSummary(const QString& markdown) {
    // 取前 1000 字符用于翻译（摘要基于翻译结果截取）
    QString plain = markdown.simplified();
    if (plain.length() > 1000) plain = plain.left(1000) + "……";
    if (plain.trimmed().isEmpty()) return QString();
    try {
        // 用翻译引擎翻译成中文，取前 200 字作为摘要
        auto r = translate_text(plain.toStdString(), "Chinese", 512);
        QString result = QString::fromStdString(r);
        if (result.length() > 200) result = result.left(200) + "……";
        return result;
    } catch (...) { return plain.left(200); }
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

void KnowledgeBasePage::onBatchDelete() {
    if (checkedDocIds_.isEmpty()) return;
    auto reply = QMessageBox::question(this, "批量删除",
        QStringLiteral("确定删除选中的 %1 个文档？").arg(checkedDocIds_.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    auto& km = KnowledgeBaseManager::getInstance();
    int deleted = 0;
    for (int id : checkedDocIds_) {
        if (km.deleteEntry(id)) deleted++;
    }
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
