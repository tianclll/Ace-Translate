# 知识库 MVP 扩展设计

## 概述

在现有知识库 MVP 基础上，对标软件内【文件翻译】页面视觉风格，扩展知识库功能：新增分类标签管理、双栏详情面板、引擎解析集成、标签多对多关联。

## 修改范围

### 新增文件（4个）
| 文件 | 内容 |
|------|------|
| `ui/knowledgebase_page.h` | 重写 KnowledgeBasePage 声明 |
| `ui/knowledgebase_page.cpp` | 重写 KnowledgeBasePage UI 实现（双栏布局、详情面板、标签交互） |
| `ui/knowledgebase_manager.h` | 扩展 KnowledgeBaseManager：标签 CRUD、文档-标签关联、引擎解析入口 |
| `ui/knowledgebase_manager.cpp` | 扩展实现 |

### 修改文件（3个）
| 文件 | 改动 |
|------|------|
| `ui/mainwindow.cpp` | `createKnowledgePanel()` 无需改动（已有），`onWorkerFinished` 的 Archive 逻辑增加标签字段传递 |
| `CMakeLists.txt` | 已有源文件路径无变化（重写替换旧文件） |
| `ui/resources.qrc` | 无需改动 |

### 重写文件（2个）
- `ui/knowledgebase_page.h` — 新增成员变量（搜索框、详情面板、标签控件等）
- `ui/knowledgebase_page.cpp` — 全新 UI 布局

## 数据库设计

### documents 表（已有，扩展字段）

```sql
CREATE TABLE IF NOT EXISTS documents (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    title       TEXT NOT NULL,
    file_type   TEXT NOT NULL DEFAULT '',
    source_path TEXT DEFAULT '',
    md_path     TEXT NOT NULL,
    lang        TEXT DEFAULT '',
    file_size   INTEGER DEFAULT 0,
    summary     TEXT DEFAULT '',        -- 新增：AI 生成摘要
    parse_status TEXT DEFAULT 'pending', -- 新增：pending/processing/done/error
    created_at  TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);
```

### tags 表（新增）

```sql
CREATE TABLE IF NOT EXISTS tags (
    id    INTEGER PRIMARY KEY AUTOINCREMENT,
    name  TEXT NOT NULL UNIQUE
);
```

### document_tags 表（新增）

```sql
CREATE TABLE IF NOT EXISTS document_tags (
    doc_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY (doc_id, tag_id),
    FOREIGN KEY (doc_id) REFERENCES documents(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);
```

### 文件存储
- markdown 文件仍为 `knowledge_base/md/<id>.md`
- 图片/PDF 等非文本文件：在 `knowledge_base/md/<id>.md` 中保存引擎解析结果（Markdown 全文）

## 页面布局（自上而下）

```
┌──────────────────────────────────────────────┐
│  QLabel#sectionTitle "个人知识库"              │
├──────────────────────────────────────────────┤
│  上传区 — DropZoneWidget                      │
│  (白色背景，2px dashed #d0d5dd，圆角14px)      │
│  📁 "拖拽文件到此处，自动解析并归档存入个人知识库" │
│  "支持 JPG/PNG / PDF / DOCX / XLSX / PPTX..." │
│  [PDF] [DOCX] [XLSX] [PPTX] [MD] [TXT] [图片] │
├──────────────────────────────────────────────┤
│  工具栏 — QFrame(白色背景圆角12px)             │
│  [🔍 搜索] [标签下拉 ▼] [+ 新建标签] [📥 批量导入] │
├──────────────┬───────────────────────────────┤
│  文档列表     │  详情面板                      │
│  width: 42%  │  flex: 1                      │
│  QFrame(白色)│  QFrame(白色)                  │
│  ┌──────────┐│  ┌─ AI 生成摘要 ─────────────┐│
│  │ doc item ││  │  (QLabel 文字)             ││
│  ├──────────┤│  ├─ Markdown 预览 ───────────┐│
│  │ doc item ││  │  (QPlainTextEdit readonly) ││
│  ├──────────┤│  ├───────────────────────────┤│
│  │ doc item ││  │  [翻译全文][导出][改标签][删除]││
│  └──────────┘│  └───────────────────────────┘│
└──────────────┴───────────────────────────────┘
```

### 上传区
- 复用/扩充 DropZoneWidget，白色背景（非 #F0F7F6），圆角14px
- 点击弹出文件选择对话框（多选）
- 拖入文件后自动调用引擎解析管线（见第8节）

### 工具栏
- 搜索输入框 — 实时过滤标题和 OCR 全文（`LIKE %keyword%`）
- 标签筛选下拉 — 选项包括"全部文档" + 所有已建标签名
- "＋ 新建标签"按钮 — 弹窗 `QInputDialog::getText`，输入标签名后写入 tags 表
- "📥 批量导入" — 弹出文件选择，多选后逐个解析入库

### 文档列表
- 白色 QFrame，scrollArea 包裹
- 每项：标题 + 元数据（日期 · 来源） + 标签 pill（绿色圆角）
- `QFrame#fileItem` 风格 hover 浅灰高亮，点击选中绿色浅底色

### 详情面板
- 白色 QFrame，scrollArea 包裹
- **AI 生成摘要** — QLabel 显示引擎生成的摘要文本
- **Markdown 预览** — QPlainTextEdit readonly，展示文档完整 Markdown
- **底部操作按钮**：
  - "🌐 翻译全文" — 调用 `translate_text()` 翻译全文 Markdown，结果替换预览
  - "📥 导出 Markdown" — 调用 `exportEntry()`
  - "🏷️ 修改标签" — 弹出标签选择列表（Checkable list），可多选
  - "🗑️ 删除" — 确认后删除

## KnowledgeBasePage 接口

```cpp
class KnowledgeBasePage : public QWidget {
    Q_OBJECT
public:
    explicit KnowledgeBasePage(QWidget* parent = nullptr);
    ~KnowledgeBasePage() override;

    void refreshList();
    void refreshTags();

signals:
    void statusMessage(const QString& msg);

private slots:
    void onFileDropped(const QStringList& paths);
    void onSearchTextChanged(const QString& text);
    void onTagFilterChanged(int index);
    void onAddNewTag();
    void onBatchImport();
    void onDocItemClicked(int id);
    void onTranslateFullText();
    void onExportMD();
    void onChangeTags();
    void onDeleteEntry();

private:
    void setupUI();
    void loadDocDetail(int id);
    QString generateSummary(const QString& markdown);  // 调用 translate_text 截取前N字符做摘要

    // UI 控件
    DropZoneWidget* dropZone_;
    QLineEdit* searchInput_;
    QComboBox* tagFilterCombo_;
    QWidget* listContainer_;
    QVBoxLayout* listLayout_;
    QScrollArea* listScroll_;
    QLabel* emptyHint_;
    QWidget* detailPanel_;
    QLabel* summaryLabel_;
    QPlainTextEdit* mdPreview_;
    int currentDocId_ = -1;

    // 缓存
    struct DocItemData {
        int id;
        QString title;
        QList<int> tagIds;
    };
    QList<DocItemData> docCache_;
};
```

## KnowledgeBaseManager 扩展

```cpp
class KnowledgeBaseManager : public QObject {
    Q_OBJECT
public:
    // 已有方法...
    
    // ---- 标签 CRUD ----
    bool addTag(const QString& name);
    bool deleteTag(int tagId);
    QList<QPair<int,QString>> getAllTags();  // id, name

    // ---- 文档-标签关联 ----
    bool setDocumentTags(int docId, const QList<int>& tagIds);
    QList<int> getDocumentTagIds(int docId);
    QStringList getDocumentTagNames(int docId);

    // ---- 引擎解析 ----
    bool parseFile(int docId, const QString& filePath, 
                   int dpi = 200, bool enableWarp = true, 
                   bool enableEnhance = false);

    // ---- 搜索 ----
    QList<KnowledgeEntry> searchEntries(const QString& keyword);
    QList<KnowledgeEntry> getEntriesByTag(int tagId);

    // ---- 摘要 ----
    bool updateSummary(int docId, const QString& summary);
    
    // ---- 已扩展的 documents 表 ----
    // summary TEXT, parse_status TEXT
};
```

## 引擎解析流程

```
parseFile(docId, filePath)
  ↓
QFileInfo fi(filePath)
  ↓ 根据扩展名路由
├─ .md / .txt → 直接读取内容 → 更新 md 文件
├─ .pdf       → process_pdf()  → 取返回 .md 路径 → 读取内容
├─ .{图片}    → process_image() → 取返回 .md 路径 → 读取内容
├─ .docx/.xlsx/.pptx → process_file() (内部 OfficeConverter) → 同上
  ↓
读取生成的 Markdown → 存入 <docId>.md
  ↓
generateSummary(markdown) → 取前 500 字 + translate_text 简译 → 存 summary 字段
  ↓
更新 parse_status = "done"
```

**注意：**
- 为避免阻塞 UI，文件解析在单独的 `QThread` 中运行
- 大型文件解析时显示进度条
- 图片/PDF 的引擎调用复用已有的 `process_image()` / `process_pdf()` / `process_file()` 等自由函数

## 文件翻译联动

`onWorkerFinished` 中的 Archive to KB 按钮保持不变，额外传递分类标签：
- 在文件面板底部参数卡片的标签下拉框中选择标签
- 点击 Archive to KB 时，将选中的标签 ID 一并存入 `document_tags`
- 归档成功后刷新知识库列表

## 实现顺序

1. 扩展 `KnowledgeBaseManager`：tags 表 + 标签 CRUD + 文档-标签关联 + searchEntries + parseFile
2. 重写 `KnowledgeBasePage`：setupUI 实现全套布局
3. 实现文件解析线程和状态反馈
4. 实现文件翻译联动的标签传递
5. 编译测试
