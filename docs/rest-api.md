# AceTranslatePro REST HTTP API 文档

AceTranslatePro 内嵌了一个 REST HTTP API 服务器（基于 Qt6 的 `QHttpServer`），供外部工具、脚本、Web 前端远程调用翻译和知识库功能。

- **启用方式**：设置面板 → *REST API Server* → 勾选 *Enable REST API*，配置端口后**重启应用生效**。对应 `config.json` 中的 `api_enabled` / `api_port` 字段（默认关闭，默认端口 `18888`）。
- **默认地址**：`http://127.0.0.1:18888`（监听所有网卡）
- **数据格式**：请求/响应均为 `application/json`
- **无鉴权**（计划后续版本加入）

---

## 约定

### 异步翻译（Job 模型）

翻译类接口（文本/文件/图片）均为**异步**：提交后立即返回 `202 Accepted` 和一个 `job_id`，真正的翻译任务在后台线程池执行。客户端通过轮询 `GET /api/jobs/{job_id}` 获取最终结果。

```
提交任务              轮询结果
POST /api/translate/text  →  GET /api/jobs/{id}
        ↓ 202 + job_id          ↓ 200 completed
```

### 通用响应字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `job_id` | string | 任务 ID，如 `job-000001` |
| `status` | string | `pending` / `running` / `completed` / `failed` / `cancelled` |
| `created_at` | string | 任务创建时间 `yyyy-MM-dd HH:mm:ss` |
| `finished_at` | string | 任务完成时间（未完成时为空串） |
| `result` | string | 翻译文本 / 输出文件路径（失败时为空） |
| `error` | string | 错误信息（成功时为空串） |
| `type` | string | 任务类型 `text` / `file` / `photo` / `import` |

### 错误响应

所有错误统一返回 JSON：`{"error": "错误描述"}`，并配合合适的 HTTP 状态码：

| 状态码 | 含义 |
|---|---|
| `400` | 参数错误 / 缺少必填字段 / 文件不存在 |
| `404` | 资源不存在（如查不到任务、文档、标签） |
| `500` | 服务器内部错误（如知识库访问失败） |

---

## 1. 健康检查

### GET `/api/health`

服务器存活探测。

**响应 200**
```json
{"status": "ok"}
```

### GET `/api/status`

返回 API 状态、监听端口、引擎加载情况、知识库文档数。

**响应 200**
```json
{
    "api_enabled": true,
    "port": 18888,
    "engines": {
        "translator": true,
        "ocr": true,
        "vlm": true,
        "asr": false
    },
    "kb_doc_count": 12
}
```

---

## 2. 文本翻译（异步）

### POST `/api/translate/text`

翻译纯文本。

**请求体**
```json
{
    "text": "Hello, how are you?",
    "target_language": "Chinese",
    "max_tokens": 512
}
```

| 字段 | 类型 | 必填 | 默认 | 说明 |
|---|---|---|---|---|
| `text` | string | ✅ | — | 待翻译文本 |
| `target_language` | string | 否 | `English` | 目标语言，如 `Chinese`、`中文` |
| `max_tokens` | int | 否 | `512` | 翻译最大 token 数 |

**响应 202**
```json
{
    "job_id": "job-000001",
    "message": "Translation started",
    "status": "pending",
    "type": "text"
}
```

**错误**
- `400`：缺少 `text` 字段

---

## 3. 文件翻译（异步）

### POST `/api/translate/file`

翻译文件（按扩展名自动路由：PDF / 图片 / Markdown / txt / Office 文档）。文件必须可被应用进程访问。

**请求体**
```json
{
    "file_path": "C:/docs/manual.pdf",
    "output_path": "C:/docs/translated/manual.md",
    "target_language": "Chinese",
    "layout_threshold": 0.5,
    "pdf_dpi": 200,
    "enable_warp": true,
    "enable_enhance": false
}
```

| 字段 | 类型 | 必填 | 默认 | 说明 |
|---|---|---|---|---|
| `file_path` | string | ✅ | — | 待翻译文件**绝对路径** |
| `output_path` | string | 否 | 源文件同目录 | 输出 `.md`/`.txt` 的**绝对路径**（含文件名）。指定后 `.md` 和同目录的 `assets_<文件名>/` 图片都写到该路径所在目录，保证 md 与图片一起保留。docx/pptx/xlsx 翻译建议指定此项 |
| `target_language` | string | 否 | `English` | 目标语言 |
| `layout_threshold` | float | 否 | `0.5` | 版面分析阈值 |
| `pdf_dpi` | int | 否 | `200` | PDF 渲染 DPI |
| `enable_warp` | bool | 否 | `true` | 图像去扭曲 |
| `enable_enhance` | bool | 否 | `false` | 图像增强 |

**响应 202**（shape 同文本翻译，`type` 为 `file`）

任务完成后，`job.result` 为**输出文件路径**（未指定 `output_path` 时为源文件同目录的 `<名称>.md`，指定时为 `output_path`）。

**错误**
- `400`：缺少 `file_path`，或**文件不存在**

---

## 4. 图片翻译（异步）

### POST `/api/translate/photo`

翻译图片中的文字（OCR → 翻译 → 渲染回图片）。图片以 **base64** 编码放入请求体。

**请求体**
```json
{
    "image_base64": "/9j/4AAQSkZJRgABAQAAAQABAAD...",
    "target_language": "Chinese",
    "max_tokens": 512
}
```

| 字段 | 类型 | 必填 | 默认 | 说明 |
|---|---|---|---|---|
| `image_base64` | string | ✅ | — | 图片文件的 base64 编码 |
| `target_language` | string | 否 | `English` | 目标语言 |
| `max_tokens` | int | 否 | `512` | 翻译最大 token 数 |

**响应 202**（shape 同文本翻译，`type` 为 `photo`）

**错误**
- `400`：缺少 `image_base64`，或数据超过 50MB

---

## 5. 任务查询与取消

### GET `/api/jobs/{job_id}`

查询单个任务的状态和结果。

**响应 200 — 完成**
```json
{
    "job_id": "job-000001",
    "type": "text",
    "status": "completed",
    "created_at": "2026-08-04 14:00:00",
    "finished_at": "2026-08-04 14:00:03",
    "result": "你好，你好吗？",
    "error": "",
    "params": {
        "text": "Hello, how are you?",
        "target_language": "Chinese"
    }
}
```

**响应 200 — 进行中**
```json
{
    "job_id": "job-000002",
    "type": "file",
    "status": "running",
    "created_at": "2026-08-04 14:01:00",
    "finished_at": "",
    "result": "",
    "error": ""
}
```

**错误**
- `404`：任务不存在

### DELETE `/api/jobs/{job_id}/cancel`

取消一个**待执行**（pending）的任务。已在执行中的任务无法取消。

**响应 200**
```json
{"job_id": "job-000001", "status": "cancelled"}
```

**错误**
- `404`：任务不存在
- `400`：任务不是 pending 状态

---

## 6. 知识库 — 文档

### GET `/api/kb/entries`

分页列出知识库文档。

**查询参数**
| 参数 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `limit` | int | `100` | 每页条数（1–500） |
| `offset` | int | `0` | 偏移量 |

**响应 200**
```json
{
    "entries": [
        {
            "id": 12,
            "title": "manual.pdf",
            "file_type": "pdf",
            "source_path": "C:/docs/manual.pdf",
            "md_file_path": "md/12.md",
            "translated_lang": "Chinese",
            "file_size": 204800,
            "created_at": "2026-08-04 10:00:00",
            "summary": "",
            "parse_status": "done",
            "assets_dir": ""
        }
    ],
    "total": 12
}
```

### GET `/api/kb/entries/{id}`

查询单个文档详情。

**响应 200**：单个文档对象（字段同上）

**错误**
- `400`：`id` 非法
- `404`：文档不存在

### POST `/api/kb/entries`

创建一条知识库文档。

**请求体**
```json
{
    "title": "report.md",
    "file_type": "md",
    "source_path": "C:/docs/report.md",
    "translated_lang": "Chinese",
    "file_size": 102400,
    "markdown_content": "# Report\n\n内容...",
    "assets_dir": ""
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `title` | string | 否 | 标题（原始文件名） |
| `file_type` | string | 否 | 扩展名 |
| `source_path` | string | 否 | 原始文件路径 |
| `translated_lang` | string | 否 | 目标语言 |
| `file_size` | int | 否 | 原始文件大小 |
| `markdown_content` | string | 否 | Markdown 正文内容 |
| `assets_dir` | string | 否 | 图片资源目录 |

**响应 201**
```json
{"id": 13, "status": "created"}
```

**错误**
- `500`：创建失败

### DELETE `/api/kb/entries/{id}`

删除单个文档。

**响应 200**
```json
{"deleted": 13}
```

**错误**
- `404`：文档不存在

### DELETE `/api/kb/entries`

批量删除文档。

**请求体**
```json
{"ids": [13, 14, 15]}
```

**响应 200**
```json
{"deleted_count": 3}
```

**错误**
- `400`：`ids` 缺失或不是数组

### GET `/api/kb/search?q=keyword`

全文搜索文档。

**查询参数**
| 参数 | 类型 | 说明 |
|---|---|---|
| `q` | string | 搜索关键词 |

**响应 200**
```json
{
    "entries": [ { "id": 12, "title": "manual.pdf", ... } ]
}
```

---

## 7. 知识库 — 标签

### GET `/api/kb/tags`

获取所有标签。

**响应 200**
```json
{
    "tags": [
        {"id": 1, "name": "research"},
        {"id": 2, "name": "work"}
    ]
}
```

### POST `/api/kb/tags`

创建标签。

**请求体**
```json
{"name": "research"}
```

**响应 201**
```json
{"id": 3, "name": "research"}
```

**错误**
- `400`：缺少 `name`

### DELETE `/api/kb/tags/{id}`

删除标签。

**响应 200**
```json
{"deleted": 3}
```

**错误**
- `400`：`id` 非法
- `404`：标签不存在

---

## 8. 知识库 — 术语表（术语注入）

### GET `/api/kb/glossary`

获取术语表。可选按语言对过滤。

**查询参数**
| 参数 | 类型 | 说明 |
|---|---|---|
| `source_lang` | string | 源语言（可选） |
| `target_lang` | string | 目标语言（可选） |

不传语言参数时返回全部术语；传了则按语言对过滤。

**响应 200**
```json
{
    "terms": [
        {"term": "GPU", "translation": "图形处理器"},
        {"term": "attention", "translation": "注意力机制"}
    ]
}
```

### POST `/api/kb/glossary`

添加术语。

**请求体**
```json
{
    "term": "GPU",
    "translation": "图形处理器",
    "source_lang": "English",
    "target_lang": "Chinese"
}
```

| 字段 | 类型 | 必填 |
|---|---|---|
| `term` | string | ✅ |
| `translation` | string | ✅ |
| `source_lang` | string | 否 |
| `target_lang` | string | 否 |

**响应 201**
```json
{
    "id": 1,
    "term": "GPU",
    "translation": "图形处理器",
    "source_lang": "English",
    "target_lang": "Chinese"
}
```

**错误**
- `400`：缺少 `term` 或 `translation`

### DELETE `/api/kb/glossary/{id}`

删除术语。

**响应 200**
```json
{"deleted": 1}
```

**错误**
- `400`：`id` 非法
- `404`：术语不存在

---

## 9. 知识库 — 导入文件（异步）

### POST `/api/kb/import`

导入文件到知识库（提取文本 → 生成 Markdown → 入库）。返回任务 ID，任务完成后可在 `GET /api/jobs/{id}` 查看结果。

**请求体**
```json
{
    "file_path": "C:/docs/report.pdf",
    "skip_md": false
}
```

| 字段 | 类型 | 必填 | 默认 | 说明 |
|---|---|---|---|---|
| `file_path` | string | ✅ | — | 待导入文件绝对路径 |
| `skip_md` | bool | 否 | `false` | 是否跳过 Markdown 生成 |

**响应 202**
```json
{
    "job_id": "job-000004",
    "message": "Import started",
    "status": "pending",
    "type": "import"
}
```

**错误**
- `400`：文件路径缺失或文件不存在

---

## 10. 语音识别（ASR）

### POST `/api/asr/recognize`

将 16kHz 16-bit mono PCM 音频转为文字（SenseVoice 模型）。同步返回，不需要轮询。

**请求体**
```json
{
    "audio_base64": "BASE64_ENCODED_AUDIO_DATA",
    "max_duration": 10
}
```

| 字段 | 类型 | 必填 | 默认 | 说明 |
|---|---|---|---|---|
| `audio_base64` | string | ✅ | — | 音频的 base64 编码（支持裸 PCM 或带 WAV 头的完整 WAV 文件，16kHz 16-bit mono） |
| `max_duration` | int | 否 | `10` | 最大识别时长（秒，1–60），超出部分截断 |

**响应 200**
```json
{
    "text": "你好世界",
    "duration_ms": 3450,
    "language": "auto"
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `text` | string | 识别出的文字，失败时为空字符串 |
| `duration_ms` | int | 实际识别的音频时长（毫秒） |
| `language` | string | 当前固定为 `"auto"`（自动检测语言） |

**错误**
- `400`：缺少 `audio_base64`，或数据超过 20MB，或处理后无有效音频数据
- `500`：ASR 引擎未加载（需要安装 ASR 模型），或识别过程中出错

**注意**
- 音频格式必须是 **16kHz、16-bit、单声道** PCM。如果是完整 WAV 文件（带 RIFF 头），服务端会自动跳过 44 字节头部。
- ASR 引擎需要模型文件：`models/ASR/model_quant.onnx`、`models/ASR/tokens.json`、`models/ASR/am.mvn`。未加载时返回 500。
- 数据大小限制 20MB（约 10 分钟的 16kHz 16-bit 音频）。

**curl 示例**
```bash
# 从 test.wav 发送语音识别
curl -X POST http://localhost:18888/api/asr/recognize \
  -H "Content-Type: application/json" \
  -d "{\"audio_base64\":\"$(base64 -w 0 test.wav)\"}"
```

---

## 快速上手（curl 示例）

```bash
# 健康检查
curl http://localhost:18888/api/health

# 文本翻译：提交
curl -X POST http://localhost:18888/api/translate/text \
  -H "Content-Type: application/json" \
  -d '{"text": "Hello world", "target_language": "Chinese"}'
# → {"job_id":"job-000001", ...}

# 轮询结果
curl http://localhost:18888/api/jobs/job-000001

# 文件翻译
curl -X POST http://localhost:18888/api/translate/file \
  -H "Content-Type: application/json" \
  -d '{"file_path": "C:/docs/manual.pdf", "target_language": "Chinese"}'

# 图片翻译
curl -X POST http://localhost:18888/api/translate/photo \
  -H "Content-Type: application/json" \
  -d "{\"image_base64\":\"$(base64 -w 0 photo.png)\",\"target_language\":\"Chinese\"}"

# 列表知识库文档
curl "http://localhost:18888/api/kb/entries?limit=100"

# 搜索
curl "http://localhost:18888/api/kb/search?q=manual"
```

用 Python 自动测试全套接口：

```bash
python311 script/test_api.py
```

---

## 端点一览

| 方法 | 路径 | 说明 | 异步 |
|---|---|---|---|
| GET | `/api/health` | 健康检查 | 否 |
| GET | `/api/status` | 服务器状态 | 否 |
| POST | `/api/translate/text` | 文本翻译 | ✅ |
| POST | `/api/translate/file` | 文件翻译 | ✅ |
| POST | `/api/translate/photo` | 图片翻译（base64） | ✅ |
| GET | `/api/jobs/{job_id}` | 查询任务 | 否 |
| DELETE | `/api/jobs/{job_id}/cancel` | 取消任务 | 否 |
| GET/POST/DELETE | `/api/kb/entries` | 文档列表 / 创建 / 批量删除 | 否 |
| GET/DELETE | `/api/kb/entries/{id}` | 文档详情 / 删除 | 否 |
| GET | `/api/kb/search` | 全文搜索 | 否 |
| GET/POST | `/api/kb/tags` | 标签列表 / 创建 | 否 |
| DELETE | `/api/kb/tags/{id}` | 删除标签 | 否 |
| GET/POST | `/api/kb/glossary` | 术语列表 / 添加 | 否 |
| DELETE | `/api/kb/glossary/{id}` | 删除术语 | 否 |
| POST | `/api/kb/import` | 导入文件 | ✅ |
| POST | `/api/asr/recognize` | 语音识别（base64 PCM） | 否 |

> 注：`<id>`、`<job_id>` 为路径参数占位符，需替换为实际值（如 `/api/kb/entries/12`）。路径参数使用尖括号语法是 Qt 6.5 的 `QHttpServer` 路由规范。
