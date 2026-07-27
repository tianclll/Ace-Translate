#include "knowledgebase_page.h"
#include "knowledgebase_manager.h"
#include "mainwindow.h"        // DropZoneWidget
#include "docmind/DocumentEngine.h"
#include <QFrame>
#include <QHBoxLayout>
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
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QTimer>
#include <QScrollBar>
#include <QSplitter>
#include <QCoreApplication>
#include <QApplication>
#include <QDir>
#include <QRegularExpression>

// ============================================================
// 构造函数 — 延迟初始化数据库
// ============================================================
KnowledgeBasePage::KnowledgeBasePage(QWidget* parent)
    : QWidget(parent) {
    setupUI();

    // 页面显示后初始化数据库并加载数据
    QTimer::singleShot(0, this, [this]() {
        KnowledgeBaseManager::getInstance().initialize();
        refreshTags();
        refreshList();
    });
}

KnowledgeBasePage::~KnowledgeBasePage() = default;

// ============================================================
// setupUI — 双栏布局（上传区 → 工具栏 → 左列表 + 右详情）
// ============================================================
void KnowledgeBasePage::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // ---- 上传区（水平紧凑横条） ----
    dropZone_ = new DropZoneWidget;
    dropZone_->setFixedHeight(64);
    dropZone_->setStyleSheet(QStringLiteral(
        "DropZoneWidget { background: #F0F7F6; border: 2px dashed #D0E8E4; border-radius: 10px; }"
        "DropZoneWidget:hover { background: #E8F5F3; border-color: #0B7C72; }"
    ));
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

    connect(dropZone_, &DropZoneWidget::fileDropped,
            this, &KnowledgeBasePage::onFileDropped);
    layout->addWidget(dropZone_);

    // ---- 工具栏 ----
    auto* toolbar = new QFrame;
    toolbar->setObjectName("card");
    toolbar->setStyleSheet(QStringLiteral(
        "QFrame#card { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 10px; }"
    ));
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);
    toolbarLayout->setSpacing(8);

    searchInput_ = new QLineEdit;
    searchInput_->setPlaceholderText("搜索标题、全文内容…");
    searchInput_->setFixedHeight(30);
    connect(searchInput_, &QLineEdit::textChanged,
            this, &KnowledgeBasePage::onSearchTextChanged);
    toolbarLayout->addWidget(searchInput_, 1);

    tagFilterCombo_ = new QComboBox;
    tagFilterCombo_->setFixedHeight(30);
    tagFilterCombo_->setMinimumWidth(110);
    connect(tagFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &KnowledgeBasePage::onTagFilterChanged);
    toolbarLayout->addWidget(tagFilterCombo_);

    auto* addTagBtn = new QPushButton("＋新建标签");
    addTagBtn->setFixedHeight(30);
    addTagBtn->setStyleSheet(
        "QPushButton { border: 1px solid #D1D5DB; border-radius: 6px;"
        " padding: 0 12px; background: transparent; color: #374151; font-size: 12px; }"
        "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
    connect(addTagBtn, &QPushButton::clicked, this, &KnowledgeBasePage::onAddNewTag);
    toolbarLayout->addWidget(addTagBtn);

    auto* batchBtn = new QPushButton("批量导入");
    batchBtn->setObjectName("primaryBtn");
    batchBtn->setFixedHeight(30);
    connect(batchBtn, &QPushButton::clicked, this, &KnowledgeBasePage::onBatchImport);
    toolbarLayout->addWidget(batchBtn);

    layout->addWidget(toolbar);

    // ---- 下半部分：水平双栏（占满剩余高度） ----
    auto* contentSplit = new QSplitter(Qt::Horizontal);
    contentSplit->setHandleWidth(4);
    contentSplit->setChildrenCollapsible(false);

    // ===== 左侧文档列表 =====
    auto* listCard = new QFrame;
    listCard->setObjectName("card");
    listCard->setStyleSheet(QStringLiteral(
        "QFrame#card { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 10px; }"
    ));
    auto* listCardLayout = new QVBoxLayout(listCard);
    listCardLayout->setContentsMargins(0, 0, 0, 0);

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
    listLayout_->setContentsMargins(8, 8, 8, 8);
    listLayout_->setSpacing(2);

    emptyHint_ = new QLabel("暂无文档\n拖拽文件到上方上传区归档");
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setStyleSheet("color: #9CA3AF; font-size: 13px; padding: 20px;"
                              " background: transparent; border: none;");
    emptyHint_->setWordWrap(true);
    listLayout_->addWidget(emptyHint_);
    listLayout_->addStretch();

    listScroll_->setWidget(listContainer_);
    listCardLayout->addWidget(listScroll_);
    contentSplit->addWidget(listCard);

    // ===== 右侧详情面板 =====
    detailPanel_ = new QFrame;
    detailPanel_->setObjectName("card");
    detailPanel_->setStyleSheet(QStringLiteral(
        "QFrame#card { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 10px; }"
    ));
    auto* detailLayout = new QVBoxLayout(detailPanel_);
    detailLayout->setContentsMargins(12, 12, 12, 12);
    detailLayout->setSpacing(8);

    // — AI 摘要卡片（带标题） —
    auto* summaryCard = new QFrame;
    summaryCard->setStyleSheet(
        "QFrame { background: #F8FAFA; border: 1px solid #E8ECEF; border-radius: 8px; padding: 10px; }");
    auto* summaryCardLayout = new QVBoxLayout(summaryCard);
    summaryCardLayout->setContentsMargins(0, 0, 0, 0);
    summaryCardLayout->setSpacing(4);

    auto* summaryTitle = new QLabel("🤖 AI 生成摘要");
    summaryTitle->setStyleSheet("font-size: 12px; font-weight: 600; color: #889096;"
                                " background: transparent; border: none;");
    summaryCardLayout->addWidget(summaryTitle);

    summaryLabel_ = new QLabel("（选择文档查看摘要）");
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setStyleSheet("font-size: 12px; color: #5B6269; line-height: 1.5;"
                                 " background: transparent; border: none;");
    summaryCardLayout->addWidget(summaryLabel_);
    detailLayout->addWidget(summaryCard);

    // — Markdown 预览卡片（带标题，占满剩余空间） —
    auto* mdCard = new QFrame;
    mdCard->setStyleSheet(
        "QFrame { background: #F8FAFA; border: 1px solid #E8ECEF; border-radius: 8px; padding: 10px; }");
    auto* mdCardLayout = new QVBoxLayout(mdCard);
    mdCardLayout->setContentsMargins(0, 0, 0, 0);
    mdCardLayout->setSpacing(4);

    auto* mdTitle = new QLabel("📝 Markdown 预览");
    mdTitle->setStyleSheet("font-size: 12px; font-weight: 600; color: #889096;"
                           " background: transparent; border: none;");
    mdCardLayout->addWidget(mdTitle);

    mdPreview_ = new QTextBrowser;
    mdPreview_->setOpenExternalLinks(true);
    mdPreview_->setOpenLinks(false);
    mdPreview_->setStyleSheet(
        "QTextBrowser { background: #FFFFFF; border: 1px solid #E8ECEF; border-radius: 6px;"
        " padding: 8px; font-size: 12px; }");
    mdCardLayout->addWidget(mdPreview_, 1);
    detailLayout->addWidget(mdCard, 1);

    // — 底部操作按钮 —
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);

    auto makeBtn = [](const QString& text, bool primary) -> QPushButton* {
        auto* btn = new QPushButton(text);
        btn->setFixedHeight(28);
        if (primary) {
            btn->setObjectName("primaryBtn");
            btn->setStyleSheet(
                "QPushButton#primaryBtn { background-color: #0B7C72; color: #FFFFFF;"
                " border: none; border-radius: 5px; padding: 0 14px;"
                " font-size: 11px; font-weight: 600; }"
                "QPushButton#primaryBtn:hover { background-color: #09685F; }");
        } else {
            btn->setStyleSheet(
                "QPushButton { border: 1px solid #D1D5DB; border-radius: 5px;"
                " padding: 0 10px; background: transparent; color: #374151; font-size: 11px; }"
                "QPushButton:hover { border-color: #0B7C72; color: #0B7C72; }");
        }
        return btn;
    };

    translateBtn_ = makeBtn("翻译全文", true);
    connect(translateBtn_, &QPushButton::clicked,
            this, &KnowledgeBasePage::onTranslateFullText);
    btnRow->addWidget(translateBtn_);

    exportBtn_ = makeBtn("导出 MD", false);
    connect(exportBtn_, &QPushButton::clicked,
            this, &KnowledgeBasePage::onExportMD);
    btnRow->addWidget(exportBtn_);

    tagBtn_ = makeBtn("修改标签", false);
    connect(tagBtn_, &QPushButton::clicked,
            this, &KnowledgeBasePage::onChangeTags);
    btnRow->addWidget(tagBtn_);

    deleteBtn_ = makeBtn("删除", false);
    deleteBtn_->setStyleSheet(
        "QPushButton { border: 1px solid #E0E0E0; border-radius: 5px;"
        " padding: 0 10px; background: transparent; color: #9CA3AF; font-size: 11px; }"
        "QPushButton:hover { color: #EF4444; border-color: #EF4444; }");
    connect(deleteBtn_, &QPushButton::clicked,
            this, &KnowledgeBasePage::onDeleteEntry);
    btnRow->addWidget(deleteBtn_);

    btnRow->addStretch();
    detailLayout->addLayout(btnRow);

    contentSplit->addWidget(detailPanel_);
    contentSplit->setSizes({260, 540});

    layout->addWidget(contentSplit, 1);
}

// ============================================================
// refreshList — 重新加载文档列表
// ============================================================
void KnowledgeBasePage::refreshList() {
    // 清除旧条目（保留 emptyHint_ 和最后的 stretch）
    while (listLayout_->count() > 2) {
        auto* item = listLayout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto& km = KnowledgeBaseManager::getInstance();

    // 根据搜索框和标签筛选获取数据
    QList<KnowledgeEntry> docs;
    QString keyword = searchInput_ ? searchInput_->text().trimmed() : QString();
    int tagFilter = tagFilterCombo_ ? tagFilterCombo_->currentData().toInt() : -1;

    if (!keyword.isEmpty())
        docs = km.searchEntries(keyword);
    else if (tagFilter > 0)
        docs = km.getEntriesByTag(tagFilter);
    else
        docs = km.getAllEntries();

    emptyHint_->setVisible(docs.isEmpty());

    for (const auto& doc : docs) {
        QStringList tagNames = km.getDocumentTagNames(doc.id);
        auto* item = createListItem(doc.id, doc.title,
                                    doc.createdAt.toString("yyyy-MM-dd"),
                                    doc.fileType.toUpper(), tagNames);
        listLayout_->insertWidget(listLayout_->count() - 1, item);
    }
}

// ============================================================
// createListItem — 创建单条文档列表项
// ============================================================
QWidget* KnowledgeBasePage::createListItem(int id, const QString& title,
                                            const QString& date,
                                            const QString& fileType,
                                            const QStringList& tags) {
    auto* item = new QFrame;
    item->setCursor(Qt::PointingHandCursor);
    item->setProperty("docId", id);
    item->setStyleSheet(
        "QFrame { background: transparent; border-radius: 8px; padding: 10px 12px; }"
        "QFrame:hover { background: #F5F7F7; }");
    item->installEventFilter(this);

    auto* layout = new QVBoxLayout(item);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    // 文档标题
    auto* titleLbl = new QLabel(title);
    titleLbl->setStyleSheet("font-weight: 500; font-size: 13px; color: #1A1A2E;"
                            " background: transparent; border: none;");
    layout->addWidget(titleLbl);

    // 元数据行：日期 · 来源 · 标签 pill
    QString tagStr = tags.isEmpty() ? "未分类" : tags.join(", ");
    auto* metaLbl = new QLabel(
        QStringLiteral("%1 · %2 <span style='background:#E8F5F3;color:#0B7C72;"
                       "border-radius:8px;padding:0 6px;font-size:10px;'>%3</span>")
            .arg(date, fileType, tagStr));
    metaLbl->setTextFormat(Qt::RichText);
    metaLbl->setStyleSheet("font-size: 11px; color: #8E8E93;"
                           " background: transparent; border: none;");
    layout->addWidget(metaLbl);

    return item;
}

// ============================================================
// eventFilter — 处理文档列表项点击选中
// ============================================================
bool KnowledgeBasePage::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (w && w->property("docId").isValid()) {
            int id = w->property("docId").toInt();
            onDocItemClicked(id);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ============================================================
// loadDocDetail — 加载文档详情到右侧面板
// ============================================================
void KnowledgeBasePage::loadDocDetail(int id) {
    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(id);
    if (doc.id < 0) return;

    currentDocId_ = id;
    currentTagIds_ = km.getDocumentTagIds(id);

    // 摘要
    if (doc.summary.isEmpty()) {
        summaryLabel_->setText(
            QStringLiteral("<span style='color:#bbb;'>（暂无摘要）</span>"));
        summaryLabel_->setTextFormat(Qt::RichText);
    } else {
        summaryLabel_->setText(doc.summary);
        summaryLabel_->setTextFormat(Qt::PlainText);
    }

    // Markdown 预览 — 用 mdToHtml 转成 HTML 后通过 QTextBrowser 显示
    mdPreview_->setHtml(mdToHtml(doc.markdownContent, doc.sourcePath));

    // 高亮列表中的选中项
    for (int i = 0; i < listLayout_->count(); ++i) {
        auto* item = listLayout_->itemAt(i);
        if (!item || !item->widget()) continue;
        if (item->widget() == emptyHint_) continue;
        bool sel = item->widget()->property("docId").toInt() == id;
        item->widget()->setProperty("selected", sel);
        if (sel) {
            item->widget()->setStyleSheet(
                "QFrame { background: #E8F5F3; border-radius: 8px; padding: 10px 12px; }");
        } else {
            item->widget()->setStyleSheet(
                "QFrame { background: transparent; border-radius: 8px; padding: 10px 12px; }"
                "QFrame:hover { background: #F5F7F7; }");
        }
    }
}

// ============================================================
// onFileDropped — 文件拖入/选择后自动解析归档
// ============================================================
void KnowledgeBasePage::onFileDropped(const QStringList& paths) {
    QStringList files = paths;
    if (files.isEmpty()) {
        // 点击上传：弹出文件选择对话框
        files = QFileDialog::getOpenFileNames(
            this, "选择文件", QString(),
            "所有支持的文件 (*.pdf *.docx *.xlsx *.pptx *.md *.txt"
            " *.png *.jpg *.jpeg *.bmp *.tiff);;所有文件 (*)");
    }
    if (files.isEmpty()) return;

    auto& km = KnowledgeBaseManager::getInstance();
    km.initialize();

    // 逐个解析入库
    int added = 0;
    for (const QString& path : files) {
        QFileInfo fi(path);
        KnowledgeEntry entry;
        entry.title = fi.fileName();
        entry.fileType = fi.suffix().toLower();
        entry.sourcePath = path;
        entry.fileSize = fi.size();

        QString ext = entry.fileType;
        // 文本文件直接读取内容
        if (ext == "md" || ext == "txt") {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                entry.markdownContent = QString::fromUtf8(f.readAll());
                f.close();
            }
        } else {
            // PDF / 图片 / Office 等非文本文件：暂不调用引擎解析（避免阻塞 UI），
            // 只存源路径，后续查看时按需解析
            entry.markdownContent = "（需解析后生成内容，源文件：" + path + "）";
        }

        int newId = -1;
        bool ok = km.addEntry(entry, &newId);
        if (ok) {
            // 生成摘要（仅文本文件有内容时）
            if (!entry.markdownContent.startsWith("（需解析后生成内容") &&
                !entry.markdownContent.trimmed().isEmpty()) {
                QString summary = generateSummary(entry.markdownContent);
                if (!summary.isEmpty()) {
                    km.updateSummary(newId, summary);
                }
            }
            added++;
        }
    }

    if (added > 0) {
        emit statusMessage(QStringLiteral("已归档 %1 个文件").arg(added));
        refreshList();
    }
}

// ============================================================
// onBatchImport — 批量导入按钮
// ============================================================
void KnowledgeBasePage::onBatchImport() {
    // 重用文件拖入逻辑（传空列表触发文件选择对话框）
    onFileDropped(QStringList());
}

// ============================================================
// generateSummary — 用引擎生成摘要
// ============================================================
QString KnowledgeBasePage::generateSummary(const QString& markdown) {
    // 取前 500 字符
    QString plain = markdown.simplified();
    if (plain.length() > 500)
        plain = plain.left(500) + "...";
    if (plain.trimmed().isEmpty())
        return QString();

    // 调用翻译引擎生成一句话摘要
    try {
        std::string result = translate_text(
            ("请用一句话概括以下内容：\n" + plain).toStdString(), "Chinese", 256);
        return QString::fromStdString(result);
    } catch (...) {
        // fallback: 取前 200 字符
        return plain.left(200);
    }
}

// ============================================================
// 工具栏交互
// ============================================================
void KnowledgeBasePage::onSearchTextChanged(const QString& text) {
    Q_UNUSED(text);
    // 防抖：300ms 内不再输入时才执行搜索
    static QTimer* debounce = nullptr;
    if (!debounce) {
        debounce = new QTimer(this);
        debounce->setSingleShot(true);
        connect(debounce, &QTimer::timeout,
                this, &KnowledgeBasePage::refreshList);
    }
    debounce->start(300);
}

void KnowledgeBasePage::onTagFilterChanged(int index) {
    Q_UNUSED(index);
    refreshList();
}

void KnowledgeBasePage::onAddNewTag() {
    bool ok;
    QString name = QInputDialog::getText(this, "新建标签",
                                          "请输入标签名称:",
                                          QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        auto& km = KnowledgeBaseManager::getInstance();
        if (km.addTag(name.trimmed())) {
            refreshTags();
            emit statusMessage(QStringLiteral("已创建标签: %1").arg(name.trimmed()));
        } else {
            emit statusMessage(QStringLiteral("创建标签失败（可能已存在）"));
        }
    }
}

// ============================================================
// refreshTags — 刷新标签下拉框
// ============================================================
void KnowledgeBasePage::refreshTags() {
    if (!tagFilterCombo_) return;
    tagFilterCombo_->blockSignals(true);
    tagFilterCombo_->clear();
    tagFilterCombo_->addItem("全部文档", -1);
    auto tags = KnowledgeBaseManager::getInstance().getAllTags();
    for (const auto& tag : tags) {
        tagFilterCombo_->addItem(tag.second, tag.first);
    }
    tagFilterCombo_->blockSignals(false);
}

// ============================================================
// 详情面板操作
// ============================================================
void KnowledgeBasePage::onDocItemClicked(int id) {
    loadDocDetail(id);
}

void KnowledgeBasePage::onTranslateFullText() {
    if (currentDocId_ < 0) return;

    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(currentDocId_);
    if (doc.id < 0 || doc.markdownContent.trimmed().isEmpty()) return;

    emit statusMessage("正在翻译全文…");
    try {
        std::string translated = translate_text(
            doc.markdownContent.toStdString(), "Chinese", 2048);
        QString result = QString::fromStdString(translated);

        mdPreview_->setMarkdown(result);

        // 更新 .md 文件
        QString mdDir = QCoreApplication::applicationDirPath()
                        + "/knowledge_base/md/";
        QFile f(mdDir + QString::number(currentDocId_) + ".md");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << result;
            f.close();
        }

        // 重新生成摘要
        QString summary = generateSummary(result);
        km.updateSummary(currentDocId_, summary);

        // 刷新详情面板
        loadDocDetail(currentDocId_);
        emit statusMessage("翻译完成");
    } catch (const std::exception& e) {
        emit statusMessage(QStringLiteral("翻译失败: %1").arg(e.what()));
    }
}

void KnowledgeBasePage::onExportMD() {
    if (currentDocId_ < 0) return;

    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(currentDocId_);
    if (doc.id < 0) return;

    QString savePath = QFileDialog::getSaveFileName(
        this, "导出 Markdown", doc.title + ".md", "Markdown (*.md)");
    if (savePath.isEmpty()) return;

    if (km.exportEntry(currentDocId_, savePath))
        emit statusMessage(QStringLiteral("已导出: %1").arg(savePath));
}

void KnowledgeBasePage::onChangeTags() {
    if (currentDocId_ < 0) return;

    auto& km = KnowledgeBaseManager::getInstance();

    // 弹窗中列出所有标签，可多选
    QDialog dlg(this);
    dlg.setWindowTitle("修改标签");
    dlg.setMinimumWidth(280);
    auto* lay = new QVBoxLayout(&dlg);

    auto allTags = km.getAllTags();
    QList<QCheckBox*> checks;
    QList<int> currentIds = km.getDocumentTagIds(currentDocId_);
    for (const auto& tag : allTags) {
        auto* cb = new QCheckBox(tag.second);
        cb->setChecked(currentIds.contains(tag.first));
        checks.append(cb);
        lay->addWidget(cb);
    }

    if (allTags.isEmpty()) {
        lay->addWidget(new QLabel("暂无标签，请先创建标签"));
    }

    auto* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btnBox);

    if (dlg.exec() == QDialog::Accepted) {
        QList<int> selected;
        for (int i = 0; i < checks.size(); ++i) {
            if (checks[i]->isChecked())
                selected.append(allTags[i].first);
        }
        km.setDocumentTags(currentDocId_, selected);
        refreshList();  // 刷新列表以更新标签显示
        emit statusMessage("标签已更新");
    }
}

void KnowledgeBasePage::onDeleteEntry() {
    if (currentDocId_ < 0) return;

    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(currentDocId_);
    if (doc.id < 0) return;

    auto reply = QMessageBox::question(this, "删除",
        QStringLiteral("确定删除 \"%1\"?").arg(doc.title),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (km.deleteEntry(currentDocId_)) {
            emit statusMessage(QStringLiteral("已删除: %1").arg(doc.title));
            currentDocId_ = -1;
            summaryLabel_->setText("（选择文档查看摘要）");
            mdPreview_->clear();
            refreshList();
        }
    }
}

// ============================================================
// mdToHtml — 将 Markdown 文本转换为 HTML
// 支持图片、表格、标题、粗体、斜体、代码、列表、链接
// ============================================================
QString KnowledgeBasePage::mdToHtml(const QString& markdown, const QString& basePath) {
    if (markdown.trimmed().isEmpty())
        return "<p style='color:#9CA3AF;margin:0;'>（空内容）</p>";

    QString css = "body{font-size:12px;line-height:1.6;color:#5B6269;margin:8px;}"
        "h1{font-size:18px;font-weight:600;margin:12px 0 4px;}"
        "h2{font-size:16px;font-weight:600;margin:12px 0 4px;}"
        "h3{font-size:15px;font-weight:600;margin:10px 0 4px;}"
        "h4{font-size:14px;margin:8px 0 4px;}h5{font-size:13px;margin:8px 0 4px;}h6{font-size:12px;margin:8px 0 4px;}"
        "p{margin:2px 0;}ul,ol{margin:2px 0;padding-left:20px;}li{margin:1px 0;}"
        "code{background:#F0F2F4;padding:1px 4px;border-radius:3px;font-size:11px;}"
        "pre{background:#F5F7F7;border:1px solid #E8ECEF;border-radius:6px;padding:10px;overflow-x:auto;}"
        "pre code{background:transparent;padding:0;border-radius:0;font-size:11px;}"
        "table{border-collapse:collapse;width:100%;margin:8px 0;}"
        "th,td{border:1px solid #D0D4D8;padding:6px 10px;text-align:left;}"
        "th{background:#F5F7F7;font-weight:600;}"
        "img{max-width:100%;border-radius:6px;margin:8px 0;}"
        "a{color:#0B7C72;}";

    QString html = "<html><head><style>" + css + "</style></head><body>";

    QStringList lines = markdown.split('\n');

    // 预处理：找出块级公式 $$...$$ 的行范围并替换为占位行
    struct FormulaBlock { int start, end; QString content; };
    QList<FormulaBlock> formulas;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed() == "$$") {
            int j = i + 1;
            while (j < lines.size() && lines[j].trimmed() != "$$") ++j;
            if (j < lines.size()) {
                QStringList parts;
                for (int k = i + 1; k < j; ++k) parts << lines[k];
                formulas.append({i, j, parts.join("\n")});
                // 标记占位
                lines[i] = QStringLiteral("<!-- FORMULA_START_%1 -->").arg(formulas.size() - 1);
                for (int k = i + 1; k <= j; ++k) lines[k] = "";
                i = j;
            }
        }
    }

    bool inCode = false, inTable = false, inList = false;
    bool lastEmpty = false;
    QString tableBuf;

    for (int i = 0; i < lines.size(); ++i) {
        QString raw = lines[i];
        QString line = raw.trimmed();

        // 代码块
        if (line.startsWith("```")) {
            if (inCode) { html += "</pre>"; inCode = false; }
            else { html += "<pre><code>"; inCode = true; }
            lastEmpty = false;
            continue;
        }
        if (inCode) { html += raw.toHtmlEscaped() + "\n"; lastEmpty = false; continue; }

        // 空行 — 关闭表格/列表，不做其它处理
        if (line.isEmpty()) {
            if (inTable) { html += "</tbody></table>"; inTable = false; tableBuf.clear(); }
            if (inList) { html += "</ul>"; inList = false; }
            continue;
        }
        lastEmpty = false;

        // 表格
        if (line.startsWith('|') && line.endsWith('|')) {
            QString t = line.mid(1, line.length() - 2);
            QStringList cells;
            for (const QString& c : t.split('|')) cells << c.trimmed();
            if (cells.size() > 0 && cells[0].contains("---")) continue;
            if (!inTable) {
                tableBuf = "<table><thead><tr>";
                for (const QString& c : cells) tableBuf += "<th>" + c.toHtmlEscaped() + "</th>";
                tableBuf += "</tr></thead><tbody>"; inTable = true;
            } else {
                tableBuf += "<tr>";
                for (const QString& c : cells) tableBuf += "<td>" + c.toHtmlEscaped() + "</td>";
                tableBuf += "</tr>";
            }
            continue;
        }
        if (inTable) { html += "</tbody></table>"; inTable = false; tableBuf.clear(); }
        if (inList) { html += "</ul>"; inList = false; }

        // 行内格式处理
        QString p = line;
        // 图片 ![](url)
        QRegularExpression imgRe("!\\[([^]]*)\\]\\(([^)]+)\\)");
        int pos = 0;
        QString imgResult;
        QRegularExpressionMatchIterator imgIt = imgRe.globalMatch(p);
        while (imgIt.hasNext()) {
            auto m = imgIt.next();
            imgResult += p.mid(pos, m.capturedStart() - pos);
            QString alt = m.captured(1).toHtmlEscaped();
            QString src = m.captured(2);
            if (!QFileInfo(src).isAbsolute() && !basePath.isEmpty())
                src = QFileInfo(basePath).absolutePath() + "/" + src;
            src = QFileInfo(src).absoluteFilePath();
            imgResult += "<img src='file:///" + src.toHtmlEscaped() + "' alt='" + alt + "'>";
            pos = m.capturedEnd();
        }
        imgResult += p.mid(pos);
        p = imgResult;

        // 链接
        QRegularExpression linkRe("\\[([^]]+)\\]\\(([^)]+)\\)");
        pos = 0; QString linkResult;
        QRegularExpressionMatchIterator linkIt = linkRe.globalMatch(p);
        while (linkIt.hasNext()) {
            auto m = linkIt.next();
            linkResult += p.mid(pos, m.capturedStart() - pos);
            linkResult += "<a href='" + m.captured(2).toHtmlEscaped() + "'>" + m.captured(1).toHtmlEscaped() + "</a>";
            pos = m.capturedEnd();
        }
        linkResult += p.mid(pos);
        p = linkResult;

        // 粗体/斜体/行内代码
        p.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<b>\\1</b>");
        p.replace(QRegularExpression("(?<!\\*)\\*(?!\\*)(.+?)(?<!\\*)\\*(?!\\*)"), "<i>\\1</i>");
        p.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
        p.replace(QRegularExpression("~~(.+?)~~"), "<s>\\1</s>");
        // 行内公式 $...$
        p.replace(QRegularExpression("\\$(.+?)\\$"), "<code style='background:#FFF3E0;color:#E65100;'>\\1</code>");

        // 检查是否是块级公式占位
        if (line.contains("<!-- FORMULA_START_")) {
            int idx = line.mid(QStringLiteral("<!-- FORMULA_START_").length()).split("-->").first().trimmed().toInt();
            if (idx >= 0 && idx < formulas.size()) {
                html += "<pre style='background:#FFF3E0;border:1px solid #FFE0B2;border-radius:6px;padding:10px;color:#E65100;font-size:12px;'>"
                     + formulas[idx].content.toHtmlEscaped() + "</pre>";
                continue;
            }
        }

        // 标题
        if (line.startsWith("###### ")) html += "<h6>" + p.mid(7) + "</h6>";
        else if (line.startsWith("##### ")) html += "<h5>" + p.mid(6) + "</h5>";
        else if (line.startsWith("#### ")) html += "<h4>" + p.mid(5) + "</h4>";
        else if (line.startsWith("### ")) html += "<h3>" + p.mid(4) + "</h3>";
        else if (line.startsWith("## ")) html += "<h2>" + p.mid(3) + "</h2>";
        else if (line.startsWith("# ")) html += "<h1>" + p.mid(2) + "</h1>";
        // 列表
        else if (line.startsWith("- ") || line.startsWith("* ") || line.startsWith("+ ")) {
            if (!inList) { html += "<ul>"; inList = true; }
            html += "<li>" + p.mid(2) + "</li>";
        }
        else if (line.length() > 2 && line[0].isDigit() && (line[1] == '.' || line[1] == ')')) {
            int dot = line.indexOf('.');
            if (dot < 0) dot = line.indexOf(')');
            if (dot > 0) {
                if (!inList) { html += "<ol>"; inList = true; }
                html += "<li>" + p.mid(dot + 1).trimmed() + "</li>";
            }
        }
        else if (line == "---" || line == "***" || line == "___") {
            html += "<hr style='border:none;border-top:1px solid #E8ECEF;margin:12px 0;'>";
        }
        else {
            html += "<p>" + p + "</p>";
        }
    }

    if (inTable) html += "</tbody></table>";
    if (inCode) html += "</code></pre>";
    if (inList) html += "</ul>";

    html += "</body></html>";
    return html;
}
