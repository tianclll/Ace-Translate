#pragma once

#include <QString>
#include <QDateTime>

namespace ApiRoutes {

// ==================== Route Paths ====================

constexpr const char* kHealth        = "/api/health";
constexpr const char* kStatus        = "/api/status";
constexpr const char* kTranslateText = "/api/translate/text";
constexpr const char* kTranslateFile = "/api/translate/file";
constexpr const char* kTranslatePhoto= "/api/translate/photo";
constexpr const char* kJobs          = "/api/jobs";
constexpr const char* kKBEntries     = "/api/kb/entries";
constexpr const char* kKBEntryDetail = "/api/kb/entries/<arg>";
constexpr const char* kKBSearch      = "/api/kb/search";
constexpr const char* kKBTags        = "/api/kb/tags";
constexpr const char* kKBGlossary    = "/api/kb/glossary";
constexpr const char* kKBImport      = "/api/kb/import";
constexpr const char* kASRRecognize  = "/api/asr/recognize";

// ==================== JSON Schema Keys ====================

namespace BodyKeys {
constexpr const char* kText            = "text";
constexpr const char* kTargetLanguage  = "target_language";
constexpr const char* kMaxTokens       = "max_tokens";
constexpr const char* kFilePath        = "file_path";
constexpr const char* kOutputPath      = "output_path";
constexpr const char* kLayoutThreshold = "layout_threshold";
constexpr const char* kPdfDpi         = "pdf_dpi";
constexpr const char* kEnableWarp     = "enable_warp";
constexpr const char* kEnableEnhance  = "enable_enhance";
constexpr const char* kImageBase64    = "image_base64";
constexpr const char* kIds            = "ids";
constexpr const char* kQ              = "q";
constexpr const char* kLimit          = "limit";
constexpr const char* kOffset         = "offset";
constexpr const char* kSourceLang     = "source_lang";
constexpr const char* kTargetLang     = "target_lang";
constexpr const char* kSkipMd         = "skip_md";
constexpr const char* kAudioBase64    = "audio_base64";
constexpr const char* kMaxDuration    = "max_duration";
constexpr const char* kTerm           = "term";
constexpr const char* kTranslation    = "translation";
constexpr const char* kName           = "name";

// Knowledge base entry body keys
constexpr const char* kTitle          = "title";
constexpr const char* kFileType       = "file_type";
constexpr const char* kSourcePath     = "source_path";
constexpr const char* kMdFilePath     = "md_file_path";
constexpr const char* kTargetLangKB   = "translated_lang";
constexpr const char* kFileSizeKB     = "file_size";
constexpr const char* kMarkdownContent = "markdown_content";
constexpr const char* kAssetsDirKB    = "assets_dir";
}

namespace RespKeys {
constexpr const char* kJobId     = "job_id";
constexpr const char* kStatus    = "status";
constexpr const char* kType      = "type";
constexpr const char* kMessage   = "message";
constexpr const char* kResult    = "result";
constexpr const char* kError     = "error";
constexpr const char* kProgress  = "progress";
constexpr const char* kCreatedAt = "created_at";
constexpr const char* kFinishedAt = "finished_at";
constexpr const char* kTotal     = "total";
constexpr const char* kEntries   = "entries";
constexpr const char* kId        = "id";
constexpr const char* kDurationMs = "duration_ms";
constexpr const char* kLanguage  = "language";
}

} // namespace ApiRoutes
