# 专有词术语库 — 设计文档

## 1. 需求概述

在翻译过程中，专业术语可能被翻译引擎误译。术语库功能允许用户导入和管理专有词及其标准译文，翻译时自动将匹配的术语注入 prompt，确保翻译结果一致、准确。

### 核心约束

- **术语库可能很大**（成百上千条），不能全塞进 prompt
- **只注入实际出现的术语**，prompt 长度与文本内容成正比，与术语库大小无关
- 翻译引擎接口（`translator_translate` DLL）**不改动**，通过 prompt 前缀注入术语

---

## 2. 数据层设计

### 2.1 数据库表

在 `knowledge.db` 中新增 `glossary_terms` 表：

```sql
CREATE TABLE IF NOT EXISTS glossary_terms (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    term        TEXT NOT NULL,          -- 原文（术语）
    translation TEXT NOT NULL,          -- 译文（标准译法）
    source_lang TEXT DEFAULT '',         -- 源语言（如 "English"）
    target_lang TEXT DEFAULT '',         -- 目标语言（如 "Chinese"）
    created_at  TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);
CREATE INDEX IF NOT EXISTS idx_glossary_lang ON glossary_terms(source_lang, target_lang);
```

### 2.2 CRUD 接口

在 `KnowledgeBaseManager` 单例中新增以下方法（`knowledgebase_manager.h`）：

```cpp
// 术语 CRUD
bool addGlossaryTerm(const QString& term, const QString& translation,
                     const QString& sourceLang, const QString& targetLang);
bool deleteGlossaryTerm(int termId);
QList<QPair<int, QString>> getAllGlossaryTerms();  // (id, "term → translation")

// 获取指定语言对的术语（供翻译引擎使用）
QList<QPair<int, QString, QString>> getGlossaryForLang(
    const QString& sourceLang, const QString& targetLang);
// 返回 (id, term, translation)
```

对应 SQL：

```sql
-- addGlossaryTerm
INSERT INTO glossary_terms (term, translation, source_lang, target_lang)
VALUES (:term, :trans, :src, :tgt)

-- deleteGlossaryTerm
DELETE FROM glossary_terms WHERE id = :id

-- getAllGlossaryTerms
SELECT id, term, translation FROM glossary_terms ORDER BY id DESC

-- getGlossaryForLang（按术语长度降序，方便前端匹配）
SELECT id, term, translation FROM glossary_terms
WHERE (source_lang = :src AND target_lang = :tgt)
   OR (source_lang = '' AND target_lang = '')  -- 通用术语（不限语言）
ORDER BY length(term) DESC
```

---

## 3. 翻译引擎集成

### 3.1 术语注入策略

在 `TranslatorEngine::translate()` 之前（即 `FileTranslationModule::translate_text()` 和 `TextTranslationModule::translate()` 中），先匹配术语，拼到 prompt 前缀。

**匹配算法**（纯文本，无正则依赖）：

```
1. 获取文本中每个词的边界（按空格/标点分词）
2. 对每条术语，检查是否出现在文本中（大小写不敏感）
3. 按术语原文长度降序排列（长词优先）
4. 取前 MAX_GLOSSARY_ITEMS 条（建议 20 条）
5. 拼入 prompt
```

**注入格式**：

```
[术语表: transformer=变换器, self-attention=自注意力机制, embedding=嵌入]
请将以下文本翻译成 Chinese：
{original_text}
```

如果无匹配术语，不注入任何内容。

### 3.2 修改点

| 文件 | 修改内容 |
|---|---|
| `include/docmind/engines/TranslatorEngine.hpp` | 新增 `setGlossaryTerms()` 接口 |
| `src/engines/TranslatorEngine.cpp` | 实现 `translate()` 时的术语注入 |
| `src/modules/FileTranslationModule.cpp` | 翻译前加载术语 |
| `src/modules/TextTranslationModule.cpp` | 翻译前加载术语 |

**TranslatorEngine 新增接口**：

```cpp
class TranslatorEngine {
public:
    // ... 现有接口 ...

    /// 设置当前翻译用的术语表（原文→译文），翻译时自动注入 prompt
    void setGlossaryTerms(const std::vector<std::pair<std::string, std::string>>& terms);
    void clearGlossary();

private:
    std::vector<std::pair<std::string, std::string>> glossary_;
};
```

**注入逻辑**（在 `translate()` 内部）：

```cpp
std::string TranslatorEngine::translate(const std::string& text,
                                        const std::string& target_language,
                                        int max_tokens) {
    if (!initialized_ || text.empty() || target_language.empty())
        return text;

    // 如果有术语，拼到 prompt 前缀
    std::string prompt;
    if (!glossary_.empty()) {
        prompt = "[术语表: ";
        for (size_t i = 0; i < glossary_.size(); ++i) {
            if (i > 0) prompt += ", ";
            prompt += glossary_[i].first + "=" + glossary_[i].second;
        }
        prompt += "]\n请将以下文本翻译成" + target_language + "：\n";
    }

    std::string full_text = prompt + text;
    const char* result = dll_.translator_translate(
        handle_, full_text.c_str(), target_language.c_str(), max_tokens);
    // ... 现有逻辑 ...
}
```

---

## 4. UI 设计

### 4.1 入口

在知识库页面工具栏增加「专有词」按钮（位于「+ 标签」旁边）。

```
[📁 拖放文件到这里归档]  [PDF] [DOCX] [XLSX] [PPTX] [MD] [TXT] [IMG]
[🔍 24]  [📅 2026/05/02] – [2026/08/02] [清空] [搜索]  [全部文档 ▼]  [+ 标签]  [专有词]
```

### 4.2 专有词管理对话框

**弹窗样式**（与项目现有 `RoundedDlg` 一致）：
- `QDialog` + `Qt::FramelessWindowHint` + `WA_TranslucentBackground`
- 容器 `QWidget#glossaryDlg`：白色背景 `#FFFFFF`，`border-radius: 14px`
- 外间距 `16px`，内边距 `20px`
- Modal 模式，在父窗口内居中

**布局**：

```
┌──────────────────────────────────────────────────────────────┐
│ ▌ 专有词管理                                       [✕]      │
│ ▌                                                             │
│ ▌ 语言对: [源语言 ▼] → [目标语言 ▼]   [+ 添加]  [📂 导入]    │
│ ▌                                                             │
│ ▌ ┌─────────────────────────────────┬──────────────────────┐ │
│ ▌ │ 术语列表                        │ 编辑面板             │ │
│ ▌ │                                 │                      │ │
│ ▌ │ ■ transformer        变换器     │ 原文: [___________]  │ │
│ ▌ │ ■ self-attention    自注意力机制 │ 译文: [___________]  │ │
│ ▌ │ ■ BERT               BERT       │                      │ │
│ ▌ │ ■ embedding          嵌入       │    [✓ 保存]  [✕ 删除] │ │
│ ▌ │                                 │                      │ │
│ ▌ │                                 │                      │ │
│ ▌ └─────────────────────────────────┴──────────────────────┘ │
│ ▌                                                             │
│ ▌ 共 4 条术语                                     [清空全部]  │
└──────────────────────────────────────────────────────────────┘
```

**按钮样式**（与现有主题一致）：
- **主按钮**（添加/保存）：`#primaryBtn` — 绿色背景 `#0B7C72`，白色文字，`border-radius: 6px`
- **次按钮**（导入/删除/清空）：`#secondaryBtn` — 透明背景，`#0B7C72` 边框和文字，`border-radius: 6px`
- **关闭按钮**（✕）：透明背景，hover 时 `#E8F0EF` 高亮，`border-radius: 6px`

**输入框样式**（与现有主题一致）：
- `border: 1px solid #DDE1E5`，`border-radius: 6px`
- Focus 时 `border-color: #0B7C72`
- 字体 `13px`，颜色 `#1A1A2E`

**功能**：
- **添加**：手动输入原文 + 译文，选择语言对
- **导入**：支持 .txt/.csv 文件导入
  - 纯文本：每行一个术语，格式 `原文 译文`（空格分隔）或 `原文\t译文`（制表符分隔）
  - CSV：`term,translation,source_lang,target_lang`
- **编辑**：点击列表项在右侧编辑面板修改
- **删除**：单个删除或清空全部
- **语言对筛选**：按源语言→目标语言过滤显示

### 4.3 导入文件格式

**纯文本（.txt）**：
```
transformer 变换器
self-attention 自注意力机制
BERT BERT
embedding 嵌入
token 词元
```

**制表符分隔（.txt）**：
```
transformer	变换器
self-attention	自注意力机制
```

**CSV（.csv）**：
```csv
term,translation,source_lang,target_lang
transformer,变换器,English,Chinese
self-attention,自注意力机制,English,Chinese
```

---

## 5. 术语匹配算法

### 5.1 匹配流程

```
输入：文本 + 术语库列表
输出：命中的术语列表（最多 20 条）

1. 对术语库按原文长度降序排列
2. 对每条术语：
   a. 在文本中搜索术语原文（大小写不敏感）
   b. 如果找到，加入命中列表
   c. 从文本中移除已匹配部分（避免重复匹配）
3. 取前 20 条返回
```

### 5.2 实现

```cpp
std::vector<std::pair<std::string, std::string>>
matchGlossaryTerms(const std::string& text,
                   const std::vector<std::tuple<int, std::string, std::string>>& terms) {
    std::vector<std::pair<std::string, std::string>> matched;
    std::string remaining = text;

    // 术语已按长度降序排列（由 SQL ORDER BY length(term) DESC）
    for (const auto& [id, term, translation] : terms) {
        // 大小写不敏感搜索
        std::string lower_term = term;
        std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(), ::tolower);
        std::string lower_remaining = remaining;
        std::transform(lower_remaining.begin(), lower_remaining.end(),
                       lower_remaining.begin(), ::tolower);

        if (lower_remaining.find(lower_term) != std::string::npos) {
            matched.emplace_back(term, translation);
            // 防止同一术语重复匹配（最多 3 次）
            if (matched.size() >= 20) break;
        }
    }
    return matched;
}
```

### 5.3 注意事项

- **长词优先**：SQL `ORDER BY length(term) DESC` 确保 "self-attention" 比 "self" 先匹配
- **大小写不敏感**：英文术语 "Transformer" 和 "transformer" 都匹配
- **上限 20 条**：即使术语库有 1000 条，每次翻译最多注入 20 条
- **跨行匹配**：术语可能跨两行（如 PDF 换行），匹配时把文本中的换行符替换为空格后再搜索

---

## 6. 文件清单

| 文件 | 改动类型 | 说明 |
|---|---|---|
| `ui/knowledgebase_manager.h` | 新增接口 | `addGlossaryTerm`, `deleteGlossaryTerm`, `getAllGlossaryTerms`, `getGlossaryForLang` |
| `ui/knowledgebase_manager.cpp` | 修改 | 新增术语表 CRUD + `createTables()` 建表 |
| `include/docmind/engines/TranslatorEngine.hpp` | 新增接口 | `setGlossaryTerms()`, `clearGlossary()` |
| `src/engines/TranslatorEngine.cpp` | 修改 | `translate()` 中注入术语 prompt |
| `ui/knowledgebase_page.h` | 新增信号 | `void openGlossaryDialog()` |
| `ui/knowledgebase_page.cpp` | 修改 | 工具栏加「专有词」按钮 |
| `ui/glossary_dialog.h` | 新建 | 专有词管理对话框 |
| `ui/glossary_dialog.cpp` | 新建 | 对话框实现 |
| `tools/fill_ts.py` | 修改 | 新增专有词相关翻译 key |
| `translations/zh_CN.ts` | 修改 | 新增翻译 |
| `translations/ja_JP.ts` | 修改 | 新增翻译 |

---

## 7. 实现顺序

1. **数据库层**：`knowledgebase_manager` 加术语表 CRUD
2. **翻译引擎**：`TranslatorEngine` 加 `setGlossaryTerms()`，`translate()` 注入 prompt
3. **UI 弹窗**：`GlossaryDialog` 完整的增删改查 + 导入
4. **集成**：知识库页面工具栏按钮 → 打开对话框
5. **i18n**：更新翻译文件

---

## 8. 验证方案

1. 添加术语 "transformer" → "变换器"，"attention" → "注意力机制"
2. 翻译文本 "The transformer model uses self-attention"
3. 检查翻译结果是否包含 "变换器" 和 "注意力机制"
4. 导入 1000 条术语 → 翻译短文本 → 确认 prompt 长度合理（只注入匹配到的几条）
5. 删除术语 → 确认翻译恢复为无术语状态
