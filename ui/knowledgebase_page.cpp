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
#include <QCoreApplication>
#include <QDir>

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
    layout->setSpacing(12);

    // ---- 标题 ----
    auto* title = new QLabel("个人知识库");
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    // ---- 上传区（白色圆角虚线框） ----
    dropZone_ = new DropZoneWidget;
    dropZone_->setFixedHeight(160);
    dropZone_->setStyleSheet(QStringLiteral(
        "DropZoneWidget { background: #FFFFFF; border: 2px dashed #d0d5dd; border-radius: 14px; }"
        "DropZoneWidget:hover { background: #f6fcfb; border-color: #16b8a6; }"
    ));
    auto* dropLayout = new QVBoxLayout(dropZone_);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(6);

    auto* dropIcon = new QLabel(QStringLiteral("\U0001F4C1"));
    dropIcon->setAlignment(Qt::AlignCenter);
    dropIcon->setStyleSheet("font-size: 32px; background: transparent;");
    dropLayout->addWidget(dropIcon);

    auto* dropText = new QLabel("拖拽文件到此处，自动解析并归档存入个人知识库");
    dropText->setAlignment(Qt::AlignCenter);
    dropText->setStyleSheet("font-size: 14px; font-weight: 600; color: #374151; background: transparent;");
    dropLayout->addWidget(dropText);

    auto* dropHint = new QLabel("支持 JPG/PNG / PDF / DOCX / XLSX / PPTX / MD / TXT");
    dropHint->setAlignment(Qt::AlignCenter);
    dropHint->setStyleSheet("font-size: 12px; color: #999; background: transparent;");
    dropLayout->addWidget(dropHint);

    // 格式 badge 行
    auto* badgeRow = new QHBoxLayout;
    badgeRow->setAlignment(Qt::AlignCenter);
    badgeRow->setSpacing(6);
    for (const QString& fmt : {"PDF", "DOCX", "XLSX", "PPTX", "MD", "TXT", "图片"}) {
        auto* badge = new QLabel(fmt);
        badge->setStyleSheet(
            "background: #F5F7F7; border: 1px solid #E8ECEF; border-radius: 10px;"
            " padding: 2px 10px; font-size: 11px; color: #6B7280;");
        badgeRow->addWidget(badge);
    }
    dropLayout->addLayout(badgeRow);

    connect(dropZone_, &DropZoneWidget::fileDropped,
            this, &KnowledgeBasePage::onFileDropped);
    layout->addWidget(dropZone_);

    // ---- 工具栏（白色圆角容器） ----
    auto* toolbar = new QFrame;
    toolbar->setStyleSheet("QFrame { background: #FFFFFF; border-radius: 12px; }");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(14, 10, 14, 10);
    toolbarLayout->setSpacing(10);

    searchInput_ = new QLineEdit;
    searchInput_->setPlaceholderText("搜索标题、OCR全文内容…");
    searchInput_->setStyleSheet(
        "QLineEdit { border: 1px solid #e0e0e0; border-radius: 8px; padding: 8px 12px; font-size: 13px; }"
        "QLineEdit:focus { border-color: #16b8a6; }");
    connect(searchInput_, &QLineEdit::textChanged,
            this, &KnowledgeBasePage::onSearchTextChanged);
    toolbarLayout->addWidget(searchInput_, 1);

    tagFilterCombo_ = new QComboBox;
    tagFilterCombo_->setStyleSheet(
        "QComboBox { border: 1px solid #e0e0e0; border-radius: 8px; padding: 8px 10px; font-size: 12px; }");
    connect(tagFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &KnowledgeBasePage::onTagFilterChanged);
    toolbarLayout->addWidget(tagFilterCombo_);

    auto* addTagBtn = new QPushButton("＋ 新建标签");
    addTagBtn->setStyleSheet(
        "QPushButton { border: 1px solid #16b8a6; border-radius: 8px; padding: 8px 14px;"
        " background: transparent; color: #16b8a6; font-size: 13px; font-weight: 500; }"
        "QPushButton:hover { background: #e6f7f5; }");
    connect(addTagBtn, &QPushButton::clicked, this, &KnowledgeBasePage::onAddNewTag);
    toolbarLayout->addWidget(addTagBtn);

    auto* batchBtn = new QPushButton("\U0001F4E5 批量导入");
    batchBtn->setStyleSheet(
        "QPushButton { background: #16b8a6; color: white; border: none; border-radius: 8px;"
        " padding: 8px 16px; font-size: 13px; font-weight: 500; }"
        "QPushButton:hover { background: #14a092; }");
    connect(batchBtn, &QPushButton::clicked, this, &KnowledgeBasePage::onBatchImport);
    toolbarLayout->addWidget(batchBtn);

    layout->addWidget(toolbar);

    // ---- 双栏主体 ----
    auto* contentWrap = new QHBoxLayout;
    contentWrap->setSpacing(14);

    // ===== 左侧文档列表 =====
    auto* listCard = new QFrame;
    listCard->setStyleSheet("QFrame { background: #FFFFFF; border-radius: 12px; }");
    auto* listCardLayout = new QVBoxLayout(listCard);
    listCardLayout->setContentsMargins(0, 0, 0, 0);

    listScroll_ = new QScrollArea;
    listScroll_->setWidgetResizable(true);
    listScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    listScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listScroll_->setMinimumHeight(100);
    listScroll_->setStyleSheet(
        "QScrollArea { border: none; background: transparent; border-radius: 12px; }"
        "QScrollBar:vertical { width: 6px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #D0D4D8; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    listContainer_ = new QWidget;
    listContainer_->setStyleSheet("QWidget { background: transparent; }");
    listLayout_ = new QVBoxLayout(listContainer_);
    listLayout_->setContentsMargins(10, 10, 10, 10);
    listLayout_->setSpacing(2);

    emptyHint_ = new QLabel("暂无文档\n拖拽文件到上方上传区归档");
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setStyleSheet("color: #9CA3AF; font-size: 13px; padding: 40px;"
                              " background: transparent; border: none;");
    emptyHint_->setWordWrap(true);
    listLayout_->addWidget(emptyHint_);
    listLayout_->addStretch();

    listScroll_->setWidget(listContainer_);
    listCardLayout->addWidget(listScroll_);
    contentWrap->addWidget(listCard, 42);  // stretch 42 → ~42% 宽度

    // ===== 右侧详情面板 =====
    detailPanel_ = new QFrame;
    detailPanel_->setStyleSheet("QFrame { background: #FFFFFF; border-radius: 12px; }");
    auto* detailLayout = new QVBoxLayout(detailPanel_);
    detailLayout->setContentsMargins(14, 14, 14, 14);
    detailLayout->setSpacing(10);

    // — AI 摘要卡片 —
    auto* summaryCard = new QFrame;
    summaryCard->setStyleSheet(
        "QFrame { border: 1px solid #eee; border-radius: 10px; padding: 12px; }");
    auto* summaryCardLayout = new QVBoxLayout(summaryCard);
    summaryCardLayout->setContentsMargins(0, 0, 0, 0);
    summaryCardLayout->setSpacing(6);

    auto* summaryTitle = new QLabel("🤖 AI 生成摘要");
    summaryTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #444;"
                                " background: transparent; border: none;");
    summaryCardLayout->addWidget(summaryTitle);

    summaryLabel_ = new QLabel("（选择文档查看摘要）");
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setStyleSheet("font-size: 12px; color: #555; line-height: 1.6;"
                                 " background: transparent; border: none;");
    summaryCardLayout->addWidget(summaryLabel_);
    detailLayout->addWidget(summaryCard);

    // — Markdown 预览卡片 —
    auto* mdCard = new QFrame;
    mdCard->setStyleSheet(
        "QFrame { border: 1px solid #eee; border-radius: 10px; padding: 12px; }");
    auto* mdCardLayout = new QVBoxLayout(mdCard);
    mdCardLayout->setContentsMargins(0, 0, 0, 0);
    mdCardLayout->setSpacing(6);

    auto* mdTitle = new QLabel("📝 Markdown 预览");
    mdTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #444;"
                           " background: transparent; border: none;");
    mdCardLayout->addWidget(mdTitle);

    mdPreview_ = new QPlainTextEdit;
    mdPreview_->setReadOnly(true);
    mdPreview_->setPlaceholderText("（选择文档查看 Markdown 内容）");
    mdPreview_->setStyleSheet(
        "QPlainTextEdit { background: #f7f8fa; border: none; border-radius: 6px;"
        " padding: 10px; font-size: 12px; font-family: 'Courier New', monospace; color: #555; }");
    mdCardLayout->addWidget(mdPreview_, 1);
    detailLayout->addWidget(mdCard, 1);

    // — 底部操作按钮 —
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    auto makeBtn = [](const QString& text, bool primary) -> QPushButton* {
        auto* btn = new QPushButton(text);
        if (primary) {
            btn->setStyleSheet(
                "QPushButton { background: #16b8a6; color: white; border: none;"
                " border-radius: 8px; padding: 7px 14px; font-size: 12px; font-weight: 500; }"
                "QPushButton:hover { background: #14a092; }");
        } else {
            btn->setStyleSheet(
                "QPushButton { background: transparent; border: 1px solid #ddd; color: #555;"
                " border-radius: 8px; padding: 7px 14px; font-size: 12px; }"
                "QPushButton:hover { border-color: #16b8a6; color: #16b8a6; }");
        }
        return btn;
    };

    translateBtn_ = makeBtn("🌐 翻译全文", true);
    connect(translateBtn_, &QPushButton::clicked,
            this, &KnowledgeBasePage::onTranslateFullText);
    btnRow->addWidget(translateBtn_);

    exportBtn_ = makeBtn("📥 导出 Markdown", false);
    connect(exportBtn_, &QPushButton::clicked,
            this, &KnowledgeBasePage::onExportMD);
    btnRow->addWidget(exportBtn_);

    tagBtn_ = makeBtn("🏷️ 修改标签", false);
    connect(tagBtn_, &QPushButton::clicked,
            this, &KnowledgeBasePage::onChangeTags);
    btnRow->addWidget(tagBtn_);

    deleteBtn_ = makeBtn("🗑️ 删除", false);
    deleteBtn_->setStyleSheet(
        "QPushButton { background: transparent; border: 1px solid #e0e0e0; color: #999;"
        " border-radius: 8px; padding: 7px 14px; font-size: 12px; }"
        "QPushButton:hover { color: #EF4444; border-color: #EF4444; }");
    connect(deleteBtn_, &QPushButton::clicked,
            this, &KnowledgeBasePage::onDeleteEntry);
    btnRow->addWidget(deleteBtn_);

    btnRow->addStretch();
    detailLayout->addLayout(btnRow);

    contentWrap->addWidget(detailPanel_, 58);  // stretch 58 → ~58% 宽度
    layout->addLayout(contentWrap, 1);
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
        "QFrame:hover { background: #f7f8fa; }");
    item->installEventFilter(this);

    auto* layout = new QVBoxLayout(item);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    // 文档标题
    auto* titleLbl = new QLabel(title);
    titleLbl->setStyleSheet("font-weight: 500; font-size: 13px; color: #1C1C1E;"
                            " background: transparent; border: none;");
    layout->addWidget(titleLbl);

    // 元数据行：日期 · 来源 · 标签 pill
    QString tagStr = tags.isEmpty() ? "未分类" : tags.join(", ");
    auto* metaLbl = new QLabel(
        QStringLiteral("%1 · %2 <span style='background:#e6f7f5;color:#16b8a6;"
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

    // Markdown 预览
    mdPreview_->setPlainText(doc.markdownContent);

    // 高亮列表中的选中项
    for (int i = 0; i < listLayout_->count(); ++i) {
        auto* item = listLayout_->itemAt(i);
        if (!item || !item->widget()) continue;
        if (item->widget() == emptyHint_) continue;
        bool sel = item->widget()->property("docId").toInt() == id;
        item->widget()->setProperty("selected", sel);
        if (sel) {
            item->widget()->setStyleSheet(
                "QFrame { background: #e6f7f5; border-radius: 8px; padding: 10px 12px; }");
        } else {
            item->widget()->setStyleSheet(
                "QFrame { background: transparent; border-radius: 8px; padding: 10px 12px; }"
                "QFrame:hover { background: #f7f8fa; }");
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
        // 文本文件直接读取
        if (ext == "md" || ext == "txt") {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                entry.markdownContent = QString::fromUtf8(f.readAll());
                f.close();
            }
        } else {
            // PDF / 图片 / Office 文件：调用项目引擎解析
            QString baseDir = QCoreApplication::applicationDirPath();
            try {
                std::string outPath;
                if (ext == "pdf") {
                    outPath = process_pdf(
                        path.toStdString(), baseDir.toStdString(),
                        "English", 0.5f, 200);
                } else {
                    outPath = process_file(
                        path.toStdString(), "", baseDir.toStdString(),
                        "English", 0.5f, 200, true, false);
                }
                QFile f(QString::fromStdString(outPath));
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    entry.markdownContent = QString::fromUtf8(f.readAll());
                    f.close();
                }
                QFile::remove(QString::fromStdString(outPath));
            } catch (const std::exception& e) {
                entry.markdownContent = QStringLiteral("解析失败: %1").arg(e.what());
            } catch (...) {
                entry.markdownContent = "解析失败: 未知错误";
            }
        }

        if (km.addEntry(&entry)) {
            // 生成摘要
            QString summary = generateSummary(entry.markdownContent);
            if (!summary.isEmpty()) {
                km.updateSummary(entry.id, summary);
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

        mdPreview_->setPlainText(result);

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
