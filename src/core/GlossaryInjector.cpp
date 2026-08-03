#include "docmind/core/GlossaryInjector.hpp"
#include "docmind/core/ConfigManager.hpp"
#include "knowledgebase_manager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <algorithm>

namespace docmind {

std::vector<std::tuple<std::string, std::string, std::string, std::string>> GlossaryInjector::allTerms_;
std::mutex GlossaryInjector::mutex_;

void GlossaryInjector::refreshFromDB() {
    auto& km = KnowledgeBaseManager::getInstance();
    km.initialize();  // 确保数据库已初始化

    // 查询所有术语（含通用术语），按长度降序
    QSqlDatabase db = QSqlDatabase::database("kb_conn");
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    if (!q.exec("SELECT term, translation, target_lang, source_lang FROM glossary_terms "
                 "ORDER BY length(term) DESC"))
        return;

    std::vector<std::tuple<std::string, std::string, std::string, std::string>> terms;
    while (q.next()) {
        terms.emplace_back(
            q.value(0).toString().toStdString(),
            q.value(1).toString().toStdString(),
            q.value(2).toString().toStdString(),
            q.value(3).toString().toStdString()
        );
    }

    std::lock_guard<std::mutex> lock(mutex_);
    allTerms_ = std::move(terms);
}

int GlossaryInjector::prepareGlossary(TranslatorEngine* translator,
                                       const std::string& targetLang,
                                       const std::string& sourceLang,
                                       const std::string& text) {
    if (!translator) return 0;

    // 检查配置：术语库功能是否启用
    auto& cfg = docmind::ConfigManager::getInstance();
    if (!cfg.getBool("enable_glossary", false)) return 0;

    std::vector<std::pair<std::string, std::string>> terms;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [term, trans, tgt, src] : allTerms_) {
            bool targetMatch = (tgt == targetLang);
            bool sourceMatch = sourceLang.empty() || (src == sourceLang || src.empty());
            if (targetMatch && sourceMatch) {
                terms.emplace_back(term, trans);
            }
        }
    }

    if (terms.empty()) return 0;

    auto matched = matchGlossaryTerms(text, terms, 20);
    if (matched.empty()) return 0;

    translator->setGlossaryTerms(matched);
    return static_cast<int>(matched.size());
}

void GlossaryInjector::clearGlossary(TranslatorEngine* translator) {
    if (translator) translator->clearGlossary();
}

} // namespace docmind
