# 知识库 MVP 扩展实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 重写知识库页面为双栏布局（左列表+右详情），扩展标签系统，集成引擎解析管线

**Architecture:** 扩展 KnowledgeBaseManager 的 DB 层（tags + document_tags 表 + 搜索/解析方法），然后重写 KnowledgeBasePage UI 为上传区→工具栏→双栏主体结构，最后串联文件翻译联动

**Tech Stack:** Qt6 Widgets, Qt6 Sql, OpenCV, ONNXRuntime (复用现有引擎)

## Global Constraints

- 所有 UI 文本使用中文（tr 原文也直接写中文）
- 不引入任何第三方库，仅用 Qt 内置模块
- 复用项目已有 OCR/文档解析/翻译工具类（`process_image`/`process_pdf`/`process_file`/`translate_text`）
- 代码带清晰中文注释

---

## 文件改动清单

### 重写（2个）
- `ui/knowledgebase_page.h` — 新增搜索框、标签控件、详情面板成员
- `ui/knowledgebase_page.cpp` — 全新双栏 UI 布局

### 修改（2个）
- `ui/knowledgebase_manager.h` — 新增标签/搜索/解析方法声明
- `ui/knowledgebase_manager.cpp` — 新增表创建 + 实现

### 无改动
- `ui/mainwindow.cpp` — `createKnowledgePanel()` 保持不变，`onWorkerFinished` 的 Archive 逻辑兼容扩展
- `CMakeLists.txt` — 源文件路径不变
- `ui/style.qss` — 复用现有 QSS 选择器

---

### Task 1: 扩展 KnowledgeBaseManager（数据库层）

**Files:**
- Modify: `ui/knowledgebase_manager.h` — 新增声明
- Modify: `ui/knowledgebase_manager.cpp` — 新增实现

**Interfaces:**
- Consumes: 已有 `KnowledgeEntry` 结构体、`addEntry`/`deleteEntry`/`getEntry`/`getAllEntries`
- Produces: 标签 CRUD、文档-标签关联、搜索、解析入口

- [ ] **Step 1: 扩展 `knowledgebase_manager.h`**

在已有类声明中追加以下方法：

```cpp
    // ---- 标签 CRUD ----
    bool addTag(const QString& name);
    bool deleteTag(int tagId);
    QList<QPair<int,QString>> getAllTags();  // <id, name>

    // ---- 文档-标签关联 ----
    bool setDocumentTags(int docId, const QList<int>& tagIds);
    QList<int> getDocumentTagIds(int docId);
    QStringList getDocumentTagNames(int docId);

    // ---- 搜索 ----
    QList<KnowledgeEntry> searchEntries(const QString& keyword);
    QList<KnowledgeEntry> getEntriesByTag(int tagId);

    // ---- 摘要 & 状态 ----
    bool updateSummary(int docId, const QString& summary);
    bool updateParseStatus(int docId, const QString& status);
```

- [ ] **Step 2: 在 `createTables()` 中新增 tags/document_tags 表**

在已有 `CREATE TABLE documents` 之后追加：

```sql
CREATE TABLE IF NOT EXISTS tags (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS document_tags (
    doc_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY (doc_id, tag_id),
    FOREIGN KEY (doc_id) REFERENCES documents(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);
```

同时给 `documents` 表加 summary 和 parse_status 列（ALTER TABLE，兼容已有数据库）：

```sql
ALTER TABLE documents ADD COLUMN summary TEXT DEFAULT '';
ALTER TABLE documents ADD COLUMN parse_status TEXT DEFAULT 'pending';
```

使用 `exec` 逐个执行，忽略列已存在的错误（SQLite 不支持 IF NOT EXISTS 在 ALTER TABLE）。

- [ ] **Step 3: 实现标签 CRUD**

```cpp
bool KnowledgeBaseManager::addTag(const QString& name) {
    if (!initialized_) return false;
    QSqlQuery q(db_);
    q.prepare("INSERT OR IGNORE INTO tags (name) VALUES (:n)");
    q.bindValue(":n", name);
    return q.exec();
}

bool KnowledgeBaseManager::deleteTag(int tagId) {
    if (!initialized_) return false;
    QSqlQuery q(db_);
    q.prepare("DELETE FROM tags WHERE id = :id");
    q.bindValue(":id", tagId);
    return q.exec();  // CASCADE 删除关联
}

QList<QPair<int,QString>> KnowledgeBaseManager::getAllTags() {
    QList<QPair<int,QString>> list;
    if (!initialized_) return list;
    QSqlQuery q(db_);
    q.exec("SELECT id, name FROM tags ORDER BY name");
    while (q.next())
        list.append({q.value(0).toInt(), q.value(1).toString()});
    return list;
}
```

- [ ] **Step 4: 实现文档-标签关联**

```cpp
bool KnowledgeBaseManager::setDocumentTags(int docId, const QList<int>& tagIds) {
    if (!initialized_) return false;
    db_.transaction();
    QSqlQuery del(db_);
    del.prepare("DELETE FROM document_tags WHERE doc_id = :d");
    del.bindValue(":d", docId);
    del.exec();
    QSqlQuery ins(db_);
    ins.prepare("INSERT INTO document_tags (doc_id, tag_id) VALUES (:d, :t)");
    for (int tid : tagIds) {
        ins.bindValue(":d", docId);
        ins.bindValue(":t", tid);
        if (!ins.exec()) { db_.rollback(); return false; }
    }
    return db_.commit();
}

QList<int> KnowledgeBaseManager::getDocumentTagIds(int docId) {
    QList<int> ids;
    QSqlQuery q(db_);
    q.prepare("SELECT tag_id FROM document_tags WHERE doc_id = :d");
    q.bindValue(":d", docId);
    if (q.exec())
        while (q.next()) ids.append(q.value(0).toInt());
    return ids;
}

QStringList KnowledgeBaseManager::getDocumentTagNames(int docId) {
    QStringList names;
    QSqlQuery q(db_);
    q.prepare("SELECT t.name FROM tags t JOIN document_tags dt ON t.id=dt.tag_id WHERE dt.doc_id=:d");
    q.bindValue(":d", docId);
    if (q.exec())
        while (q.next()) names.append(q.value(0).toString());
    return names;
}
```

- [ ] **Step 5: 实现搜索**

```cpp
QList<KnowledgeEntry> KnowledgeBaseManager::searchEntries(const QString& keyword) {
    QList<KnowledgeEntry> list;
    if (!initialized_ || keyword.trimmed().isEmpty()) return getAllEntries();
    QSqlQuery q(db_);
    q.prepare("SELECT id, title, file_type, source_path, md_path, lang, file_size, summary, created_at "
              "FROM documents WHERE title LIKE :kw OR summary LIKE :kw2 ORDER BY id DESC");
    QString like = "%" + keyword.trimmed() + "%";
    q.bindValue(":kw", like);
    q.bindValue(":kw2", like);
    if (!q.exec()) return list;
    while (q.next()) {
        KnowledgeEntry e;
        e.id = q.value(0).toInt();
        e.title = q.value(1).toString();
        e.fileType = q.value(2).toString();
        e.sourcePath = q.value(3).toString();
        e.mdFilePath = q.value(4).toString();
        e.translatedLang = q.value(5).toString();
        e.fileSize = q.value(6).toLongLong();
        e.createdAt = QDateTime::fromString(q.value(8).toString(), "yyyy-MM-dd hh:mm:ss");
        list.append(e);
    }
    return list;
}

QList<KnowledgeEntry> KnowledgeBaseManager::getEntriesByTag(int tagId) {
    QList<KnowledgeEntry> list;
    if (!initialized_) return list;
    QSqlQuery q(db_);
    q.prepare("SELECT d.id, d.title, d.file_type, d.source_path, d.md_path, d.lang, d.file_size, d.summary, d.created_at "
              "FROM documents d JOIN document_tags dt ON d.id=dt.doc_id WHERE dt.tag_id=:tid ORDER BY d.id DESC");
    q.bindValue(":tid", tagId);
    if (!q.exec()) return list;
    while (q.next()) {
        KnowledgeEntry e;
        e.id = q.value(0).toInt();
        e.title = q.value(1).toString();
        e.fileType = q.value(2).toString();
        e.sourcePath = q.value(3).toString();
        e.mdFilePath = q.value(4).toString();
        e.translatedLang = q.value(5).toString();
        e.fileSize = q.value(6).toLongLong();
        e.createdAt = QDateTime::fromString(q.value(8).toString(), "yyyy-MM-dd hh:mm:ss");
        list.append(e);
    }
    return list;
}

bool KnowledgeBaseManager::updateSummary(int docId, const QString& summary) {
    if (!initialized_) return false;
    QSqlQuery q(db_);
    q.prepare("UPDATE documents SET summary=:s WHERE id=:id");
    q.bindValue(":s", summary);
    q.bindValue(":id", docId);
    return q.exec();
}

bool KnowledgeBaseManager::updateParseStatus(int docId, const QString& status) {
    if (!initialized_) return false;
    QSqlQuery q(db_);
    q.prepare("UPDATE documents SET parse_status=:s WHERE id=:id");
    q.bindValue(":s", status);
    q.bindValue(":id", docId);
    return q.exec();
}
```

- [ ] **Step 6: 扩展 `getAllEntries` 和 `getEntry`**

在 SQL 查询中增加 `summary` 字段（第 7 列），并读取赋值。

```cpp
// getAllEntries 中:
"SELECT id, title, file_type, source_path, md_path, lang, file_size, summary, created_at "
// 然后:
e.summary = q.value(7).toString();
e.createdAt = QDateTime::fromString(q.value(8).toString(), "yyyy-MM-dd hh:mm:ss");

// getEntry 中同理:
"SELECT id, title, file_type, source_path, md_path, lang, file_size, summary, created_at "
// ... 
e.summary = q.value(7).toString();
```

同时给 `KnowledgeEntry` 结构体新增字段：

```cpp
struct KnowledgeEntry {
    // ... 已有字段不变
    QString summary;       // AI 生成摘要
    QString parseStatus;   // pending/processing/done/error
};
```

- [ ] **Step 7: Commit**

```bash
git add ui/knowledgebase_manager.h ui/knowledgebase_manager.cpp
git commit -m "feat(kb): 扩展数据库层 — 标签 CRUD、文档-标签关联、搜索、摘要字段"
```

---

### Task 2: 重写 KnowledgeBasePage 头文件

**Files:**
- Rewrite: `ui/knowledgebase_page.h`

**Interfaces:**
- Consumes: 无（纯声明）
- Produces: 供 Task 3 实现所有方法

- [ ] **Step 1: 写入完整头文件**

```cpp
#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QList>

class DropZoneWidget;

class KnowledgeBasePage : public QWidget {
    Q_OBJECT
public:
    explicit KnowledgeBasePage(QWidget* parent = nullptr);
    ~KnowledgeBasePage() override;

    /// 刷新文档列表（从 DB 重载）
    void refreshList();
    /// 刷新标签下拉框
    void refreshTags();

signals:
    void statusMessage(const QString& msg);

private slots:
    // 上传
    void onFileDropped(const QStringList& paths);
    void onBatchImport();
    // 文档操作
    void onDocItemClicked(int id);
    void onDeleteEntry();
    void onExportMD();
    void onTranslateFullText();
    // 标签
    void onAddNewTag();
    void onChangeTags();
    // 搜索/筛选
    void onSearchTextChanged(const QString& text);
    void onTagFilterChanged(int index);

private:
    void setupUI();
    /// 加载指定文档到详情面板
    void loadDocDetail(int id);
    /// 用已有引擎生成摘要（取 md 前 500 字翻译）
    QString generateSummary(const QString& markdown);

    // ---- UI 控件 ----
    DropZoneWidget* dropZone_ = nullptr;
    QLineEdit* searchInput_ = nullptr;
    QComboBox* tagFilterCombo_ = nullptr;

    // 左侧列表
    QWidget* listContainer_ = nullptr;
    QVBoxLayout* listLayout_ = nullptr;
    QScrollArea* listScroll_ = nullptr;
    QLabel* emptyHint_ = nullptr;

    // 右侧详情
    QWidget* detailPanel_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QPlainTextEdit* mdPreview_ = nullptr;
    QPushButton* translateBtn_ = nullptr;
    QPushButton* exportBtn_ = nullptr;
    QPushButton* tagBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;

    // 状态
    int currentDocId_ = -1;
    /// 缓存选中文档的标签 ID，用于修改标签弹窗
    QList<int> currentTagIds_;
};
```

- [ ] **Step 2: Commit**

```bash
git add ui/knowledgebase_page.h
git commit -m "feat(kb): 重写 KnowledgeBasePage 头文件 — 双栏布局控件声明"
```

---

### Task 3: 重写 KnowledgeBasePage 实现（UI 布局）

**Files:**
- Rewrite: `ui/knowledgebase_page.cpp`

**Interfaces:**
- Consumes: `KnowledgeBaseManager` 的全部方法、`DropZoneWidget`、`process_image`/`process_pdf`/`process_file`（DocumentEngine.h）、`translate_text`
- Produces: 完整的知识库页面 UI

- [ ] **Step 1: 编写 `setupUI()` — 完整布局**

布局结构（自上而下）：

```
QVBoxLayout(panel)
├─ QLabel "个人知识库" (#sectionTitle)
├─ DropZoneWidget (上传区)
│   ├─ 📁 图标 (QLabel)
│   ├─ "拖拽文件到此处，自动解析并归档存入个人知识库"
│   ├─ "支持 JPG/PNG / PDF / DOCX / XLSX / PPTX / MD / TXT"
│   └─ 格式 badge 行 [PDF] [DOCX] [XLSX] [PPTX] [MD] [TXT] [图片]
├─ QFrame (工具栏, 白色圆角12px)
│   ├─ QHBoxLayout
│   │   ├─ QLineEdit (搜索, placeholder "搜索标题、OCR全文…")
│   │   ├─ QComboBox (标签筛选: "全部文档" + 标签列表)
│   │   ├─ QPushButton "＋ 新建标签"
│   │   └─ QPushButton "📥 批量导入" (#primaryBtn)
├─ QHBoxLayout (双栏)
│   ├─ QFrame (左侧列表, 白色圆角12px, fixedWidth 42% width)
│   │   ├─ QScrollArea
│   │   │   └─ listContainer_ (QVBoxLayout)
│   │   │       ├─ emptyHint_ (QLabel, 居中 "暂无文档")
│   │   │       └─ 文档条目 (动态添加)
│   │   └─ (stretch)
│   └─ QFrame (右侧详情, 白色圆角12px)
│       ├─ QScrollArea
│       │   ├─ QVBoxLayout
│       │   │   ├─ QFrame.card
│       │   │   │   ├─ QLabel "🤖 AI 生成摘要" (card-title)
│       │   │   │   └─ summaryLabel_ (QLabel, 文字样式)
│       │   │   ├─ QFrame.card
│       │   │   │   ├─ QLabel "📝 Markdown 预览"
│       │   │   │   └─ mdPreview_ (QPlainTextEdit, readonly)
│       │   │   └─ bottomButtons (QHBoxLayout)
│       │   │       ├─ translateBtn_ "🌐 翻译全文"
│       │   │       ├─ exportBtn_ "📥 导出 Markdown"
│       │   │       ├─ tagBtn_ "🏷️ 修改标签"
│       │   │       └─ deleteBtn_ "🗑️ 删除"
│       └─ (stretch)
```

样式细节：
- 上传区：白色背景，2px dashed `#d0d5dd`，圆角14px，hover 变 `#16b8a6`
- 工具栏：白色背景，圆角12px，padding 10px 14px，子控件间距10px
- 文档列表每项：padding 10px 12px，圆角8px，hover 背景 `#f7f8fa`，选中 `#e6f7f5`
- 详情面板卡片：1px solid `#eee`，圆角10px，padding 12px
- Markdown 预览：背景 `#f7f8fa`，圆角6px，padding 10px，monospace 字体
- 底部按钮：`btn` 样式 `#16b8a6` 绿色实心，`btn-outline` 透明边框

- [ ] **Step 2: 实现 `refreshList()`**

```cpp
void KnowledgeBasePage::refreshList() {
    // 清除旧条目（保留 emptyHint_ 和 stretch）
    while (listLayout_->count() > 2) {
        auto* item = listLayout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    // 获取数据（考虑搜索和标签筛选）
    QList<KnowledgeEntry> docs;
    QString keyword = searchInput_ ? searchInput_->text().trimmed() : QString();
    int tagFilter = tagFilterCombo_ ? tagFilterCombo_->currentData().toInt() : -1;

    auto& km = KnowledgeBaseManager::getInstance();
    if (!keyword.isEmpty())
        docs = km.searchEntries(keyword);
    else if (tagFilter > 0)
        docs = km.getEntriesByTag(tagFilter);
    else
        docs = km.getAllEntries();

    emptyHint_->setVisible(docs.isEmpty());

    for (const auto& doc : docs) {
        auto* item = createListItem(doc);
        listLayout_->insertWidget(listLayout_->count() - 1, item);
    }
}
```

`createListItem` 方法 — 返回单条文档的 QFrame 小部件：

```cpp
QWidget* KnowledgeBasePage::createListItem(const KnowledgeEntry& doc) {
    auto* item = new QFrame;
    item->setCursor(Qt::PointingHandCursor);
    item->setStyleSheet(
        "QFrame { background: transparent; border-radius: 8px; padding: 10px 12px; }"
        "QFrame:hover { background: #f7f8fa; }");
    // 选中状态通过属性动态控制
    item->setProperty("docId", doc.id);

    auto* layout = new QVBoxLayout(item);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    auto* titleLbl = new QLabel(doc.title);
    titleLbl->setStyleSheet("font-weight: 500; font-size: 13px; color: #1C1C1E; background: transparent; border: none;");
    layout->addWidget(titleLbl);

    // 元数据行: 日期 · 来源 · 标签
    QStringList tagNames = KnowledgeBaseManager::getInstance().getDocumentTagNames(doc.id);
    QString tagStr = tagNames.isEmpty() ? "未分类" : tagNames.join(", ");
    auto* metaLbl = new QLabel(
        QStringLiteral("%1 · %2 <span style='background:#e6f7f5;color:#16b8a6;border-radius:8px;padding:0 6px;font-size:10px;'>%3</span>")
            .arg(doc.createdAt.toString("yyyy-MM-dd"),
                 doc.fileType.toUpper(),
                 tagStr));
    metaLbl->setTextFormat(Qt::RichText);
    metaLbl->setStyleSheet("font-size: 11px; color: #8E8E93; background: transparent; border: none;");
    layout->addWidget(metaLbl);

    // 点击打开详情
    QPointer<KnowledgeBasePage> self = this;
    int id = doc.id;
    // 使用 eventFilter 在父级处理
    item->installEventFilter(this);

    return item;
}
```

注意：选中高亮用 `eventFilter` 检测 `MouseButtonPress`，设 `selected` 属性统一处理。

- [ ] **Step 3: 实现 `loadDocDetail(int id)`**

```cpp
void KnowledgeBasePage::loadDocDetail(int id) {
    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(id);
    if (doc.id < 0) return;

    currentDocId_ = id;
    currentTagIds_ = km.getDocumentTagIds(id);

    // 摘要
    summaryLabel_->setText(doc.summary.isEmpty()
        ? QStringLiteral("<span style='color:#bbb;'>(暂无摘要，点击\"翻译全文\"生成)</span>")
        : doc.summary);

    // Markdown 预览
    mdPreview_->setPlainText(doc.markdownContent);

    // 高亮列表中的选中项
    for (int i = 0; i < listLayout_->count(); ++i) {
        auto* item = listLayout_->itemAt(i);
        if (!item || !item->widget()) continue;
        bool sel = item->widget()->property("docId").toInt() == id;
        item->widget()->setProperty("selected", sel);
        item->widget()->style()->unpolish(item->widget());
        item->widget()->style()->polish(item->widget());
    }
}
```

- [ ] **Step 4: 实现上传和解析**

`onFileDropped` 重写：

```cpp
void KnowledgeBasePage::onFileDropped(const QStringList& paths) {
    QStringList files = paths;
    if (files.isEmpty()) {
        files = QFileDialog::getOpenFileNames(
            this, tr("选择文件"), QString(),
            tr("所有支持的文件 (*.pdf *.docx *.xlsx *.pptx *.md *.txt *.png *.jpg *.jpeg *.bmp *.tiff);;所有文件 (*)"));
    }
    if (files.isEmpty()) return;

    auto& km = KnowledgeBaseManager::getInstance();
    km.initialize();

    // 逐个解析入库（简单版：直接调用引擎，大文件可能卡 UI — 后续可移入工作线程）
    int added = 0;
    for (const QString& path : files) {
        QFileInfo fi(path);
        KnowledgeEntry entry;
        entry.title = fi.fileName();
        entry.fileType = fi.suffix().toLower();
        entry.sourcePath = path;
        entry.fileSize = fi.size();

        // 根据类型调用不同引擎
        QString ext = entry.fileType;
        if (ext == "md" || ext == "txt") {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                entry.markdownContent = QString::fromUtf8(f.readAll());
                f.close();
            }
        } else if (ext == "pdf") {
            // 调用 project 引擎 — 需要 baseDir + targetLang
            // 用默认英文目标语言，用户后续可重新翻译
            auto& ctx = GlobalEngineContext::getInstance();
            try {
                std::string outPath = process_pdf(path.toStdString(),
                    ctx.baseDir().toStdString(), "English", 0.5f, 200);
                QFile f(QString::fromStdString(outPath));
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    entry.markdownContent = QString::fromUtf8(f.readAll());
                    f.close();
                }
                QFile::remove(QString::fromStdString(outPath));
            } catch (...) {
                entry.markdownContent = QStringLiteral("解析失败: %1").arg(path);
            }
        } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tiff" ||
                   ext == "docx" || ext == "xlsx" || ext == "pptx") {
            // 图片/Office 文件
            auto& ctx = GlobalEngineContext::getInstance();
            try {
                std::string outPath = process_file(path.toStdString(), "",
                    ctx.baseDir().toStdString(), "English", 0.5f, 200, true, false);
                QFile f(QString::fromStdString(outPath));
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    entry.markdownContent = QString::fromUtf8(f.readAll());
                    f.close();
                }
                QFile::remove(QString::fromStdString(outPath));
            } catch (...) {
                entry.markdownContent = QStringLiteral("解析失败: %1").arg(path);
            }
        } else {
            entry.markdownContent = QStringLiteral("不支持的格式: %1").arg(ext);
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
```

注意：上面用到了 `GlobalEngineContext::getInstance().baseDir()` — 检查该 API 是否存在。如果不存在，则文件翻译时已由 MainWindow 传入 baseDir，知识库此处可用 `QCoreApplication::applicationDirPath()` 替代。

`generateSummary`：

```cpp
QString KnowledgeBasePage::generateSummary(const QString& markdown) {
    // 取前 500 字做摘要
    QString plain = markdown.simplified();
    if (plain.length() > 500)
        plain = plain.left(500) + "...";
    if (plain.trimmed().isEmpty())
        return QString();

    // 调用翻译引擎简译（目标语言设为同源语言，实际做提取摘要）
    // 实际用 translate_text 翻译为中文（摘要）
    try {
        std::string result = docmind::translate_text(
            ("请用一句话概括以下内容：\n" + plain).toStdString(), "Chinese", 256);
        return QString::fromStdString(result);
    } catch (...) {
        return plain.left(200);  // fallback: 取前 200 字
    }
}
```

- [ ] **Step 5: 实现工具栏交互**

搜索框 `onSearchTextChanged`：

```cpp
void KnowledgeBasePage::onSearchTextChanged(const QString& text) {
    Q_UNUSED(text);
    // 防抖：用 QTimer::singleShot 延迟 300ms
    static QTimer* debounce = nullptr;
    if (!debounce) {
        debounce = new QTimer(this);
        debounce->setSingleShot(true);
        connect(debounce, &QTimer::timeout, this, &KnowledgeBasePage::refreshList);
    }
    debounce->start(300);
}
```

标签筛选：

```cpp
void KnowledgeBasePage::onTagFilterChanged(int index) {
    Q_UNUSED(index);
    refreshList();
}
```

新建标签：

```cpp
void KnowledgeBasePage::onAddNewTag() {
    bool ok;
    QString name = QInputDialog::getText(this, tr("新建标签"),
                                          tr("请输入标签名称:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        auto& km = KnowledgeBaseManager::getInstance();
        if (km.addTag(name.trimmed())) {
            refreshTags();
            emit statusMessage(QStringLiteral("已创建标签: %1").arg(name.trimmed()));
        }
    }
}
```

批量导入：

```cpp
void KnowledgeBasePage::onBatchImport() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("批量导入文件"), QString(),
        tr("所有支持的文件 (*.pdf *.docx *.xlsx *.pptx *.md *.txt *.png *.jpg *.jpeg *.bmp *.tiff);;所有文件 (*)"));
    if (!files.isEmpty()) {
        onFileDropped(files);
    }
}
```

- [ ] **Step 6: 实现详情面板操作按钮**

翻译全文：

```cpp
void KnowledgeBasePage::onTranslateFullText() {
    if (currentDocId_ < 0) return;
    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(currentDocId_);
    if (doc.id < 0 || doc.markdownContent.trimmed().isEmpty()) return;

    // 异步翻译（用 translate_text 翻译整篇）
    emit statusMessage(QStringLiteral("正在翻译…"));
    try {
        std::string translated = docmind::translate_text(
            doc.markdownContent.toStdString(), "Chinese", 2048);
        QString result = QString::fromStdString(translated);
        // 更新详情面板预览
        mdPreview_->setPlainText(result);
        // 更新 .md 文件
        QString mdDir = QCoreApplication::applicationDirPath() + "/knowledge_base/md/";
        QFile f(mdDir + QString::number(currentDocId_) + ".md");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << result;
            f.close();
        }
        // 刷新摘要
        QString summary = generateSummary(result);
        km.updateSummary(currentDocId_, summary);
        emit statusMessage(QStringLiteral("翻译完成"));
    } catch (const std::exception& e) {
        emit statusMessage(QStringLiteral("翻译失败: %1").arg(e.what()));
    }
}
```

导出 Markdown：

```cpp
void KnowledgeBasePage::onExportMD() {
    if (currentDocId_ < 0) return;
    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(currentDocId_);
    if (doc.id < 0) return;

    QString savePath = QFileDialog::getSaveFileName(
        this, tr("导出 Markdown"),
        doc.title + ".md", tr("Markdown (*.md)"));
    if (savePath.isEmpty()) return;
    if (km.exportEntry(currentDocId_, savePath))
        emit statusMessage(QStringLiteral("已导出: %1").arg(savePath));
}
```

修改标签：

```cpp
void KnowledgeBasePage::onChangeTags() {
    if (currentDocId_ < 0) return;
    auto& km = KnowledgeBaseManager::getInstance();

    // 弹窗列出所有标签，可多选
    QDialog dlg(this);
    dlg.setWindowTitle(tr("修改标签"));
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
        lay->addWidget(new QLabel(tr("暂无标签，请先创建标签")));
    }

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
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
        emit statusMessage(QStringLiteral("标签已更新"));
    }
}
```

删除：

```cpp
void KnowledgeBasePage::onDeleteEntry() {
    if (currentDocId_ < 0) return;
    auto& km = KnowledgeBaseManager::getInstance();
    auto doc = km.getEntry(currentDocId_);
    if (doc.id < 0) return;

    auto reply = QMessageBox::question(this, tr("删除"),
                                        tr("确定删除 \"%1\"?").arg(doc.title),
                                        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        if (km.deleteEntry(currentDocId_)) {
            emit statusMessage(QStringLiteral("已删除: %1").arg(doc.title));
            currentDocId_ = -1;
            summaryLabel_->clear();
            mdPreview_->clear();
            refreshList();
        }
    }
}
```

- [ ] **Step 7: 添加 `qrc` 包含和构造函数**

构造函数做延迟初始化：

```cpp
KnowledgeBasePage::KnowledgeBasePage(QWidget* parent)
    : QWidget(parent) {
    setupUI();

    QTimer::singleShot(0, this, [this]() {
        KnowledgeBaseManager::getInstance().initialize();
        refreshTags();
        refreshList();
    });
}
```

记得包含 `QInputDialog`、`QFileDialog`、`QMessageBox`、`QCheckBox`、`QDialogButtonBox`、`QDialog` 等头文件。

`refreshTags`：

```cpp
void KnowledgeBasePage::refreshTags() {
    if (!tagFilterCombo_) return;
    tagFilterCombo_->blockSignals(true);
    tagFilterCombo_->clear();
    tagFilterCombo_->addItem(tr("全部文档"), -1);
    auto tags = KnowledgeBaseManager::getInstance().getAllTags();
    for (const auto& tag : tags) {
        tagFilterCombo_->addItem(tag.second, tag.first);
    }
    tagFilterCombo_->blockSignals(false);
}
```

- [ ] **Step 8: 实现 `eventFilter` 处理文档项点击高亮**

在 `KnowledgeBasePage` 类中重写 `eventFilter`：

```cpp
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
```

不要忘记在头文件中声明 `eventFilter` 重写。

- [ ] **Step 9: Commit**

```bash
git add ui/knowledgebase_page.cpp
git commit -m "feat(kb): 重写知识库页面为双栏布局 + 标签管理 + 引擎解析"
```

---

### Task 4: 文件翻译联动 — 归档时传递标签

**Files:**
- Modify: `ui/mainwindow.cpp` — `onWorkerFinished` 中 Archive 按钮逻辑

**Interfaces:**
- Consumes: `KnowledgeBaseManager::setDocumentTags()`

- [ ] **Step 1: 修改 Archive 按钮回调**

在 `onWorkerFinished` case 4 的 Archive to KB 逻辑中，归档成功后调用：

```cpp
// 归档成功后，将当前文件面板的语言下拉框文本作为默认标签（可选）
if (fileLangCombo_) {
    QString langText = fileLangCombo_->currentText();
    // 如果该标签不存在则创建
    auto& km = KnowledgeBaseManager::getInstance();
    if (!langText.isEmpty()) {
        km.addTag(langText);
        auto tags = km.getAllTags();
        for (const auto& tag : tags) {
            if (tag.second == langText) {
                km.setDocumentTags(newId, {tag.first});
                break;
            }
        }
    }
}
```

注意：此处的 `newId` 需要从 `km.addEntry()` 的返回值中获取 — 修改现有代码将 `addEntry` 调用改为获取 outId：

```cpp
int newId = -1;
if (km.addEntry(entry, &newId)) {  // 修改为传入 &newId
    // ... 现有逻辑，然后 setDocumentTags
}
```

- [ ] **Step 2: Commit**

```bash
git add ui/mainwindow.cpp
git commit -m "feat(kb): 文件翻译归档到知识库时自动创建语言标签"
```

---

### Task 5: 编译测试

**Files:**
- Build: 全套 CMake 构建

- [ ] **Step 1: 构建项目**

```bash
cd d:/AceTranslatePro/AceTranslatePro/build_all
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

- [ ] **Step 2: 运行验证**

1. 启动 `build_all/Release/AceTranslatePro.exe`
2. 点击知识库导航按钮，确认页面正常显示
3. 拖拽文件到上传区，确认引擎解析并归档
4. 点击文档列表项，确认右侧详情面板显示摘要 + Markdown
5. 新建标签、修改标签、标签筛选正常工作
6. 搜索框输入关键词过滤文档
7. 导出 Markdown、翻译全文、删除功能正常
8. 文件翻译完成后，点击 Archive to KB，确认归档成功
9. 重启 app，数据持久化

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat(kb): 知识库 MVP 扩展完成 — 双栏布局 + 标签系统 + 引擎集成"
```
