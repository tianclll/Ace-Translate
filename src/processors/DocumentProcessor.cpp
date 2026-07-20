#define NOMINMAX
#include "docmind/processors/DocumentProcessor.hpp"
#include "docmind/utils/FormatFixer.hpp"
#include "docmind/utils/ImageUtils.hpp"
#include <iostream>
#include <algorithm>
#include <limits>

namespace docmind {

// ---------- 构造函数 ----------
    DocumentProcessor::DocumentProcessor(GlobalEngineContext& ctx, const ProcessorConfig& config)
            : ctx_(ctx), config_(config) {
        std::cout << "DocumentProcessor initialized." << std::endl;
    }

// ---------- 析构函数 ----------
    DocumentProcessor::~DocumentProcessor() {
        // 无需手动清理，由全局上下文管理
    }

// ---------- 辅助判断函数 ----------
    bool DocumentProcessor::isTextType(const std::string& label) const {
        return (label == "text" || label == "content" || label == "abstract" ||
                label == "aside_text" || label == "reference_content" || label == "vertical_text");
    }

    bool DocumentProcessor::isTitleType(const std::string& label) const {
        return (label == "doc_title" || label == "paragraph_title");
    }

// ---------- 独立公式识别 ----------
    std::string DocumentProcessor::recognizeFormula(const cv::Mat& roi) {
        ctx_.ensureVLMEngine();
        auto* vlm = ctx_.getVLMEngine();
        if (!vlm) return "";
        std::string latex = vlm->recognize(roi, "Table Recognition:", 128);
        latex = FormatFixer::onlineFormulaFix(latex);
        return latex;
    }

// ---------- 处理文本块（含内部公式） ----------
    std::string DocumentProcessor::processTextBlock(const cv::Mat& image,
                                                    const BoxInfo& text_box,
                                                    const std::vector<BoxInfo>& formula_boxes) {
        cv::Mat roi = image(text_box.bbox).clone();
        if (roi.empty()) return "";

        ctx_.ensureOCREngine();
        ctx_.ensureVLMEngine();

        auto* ocr = ctx_.getOCREngine();
        auto* vlm = ctx_.getVLMEngine();
        if (!ocr) {
            std::cerr << "OCR engine not available." << std::endl;
            return "";
        }

        // 找出该文本框内的公式框（局部坐标）
        std::vector<cv::Rect> local_formula_rects;
        for (const auto& fb : formula_boxes) {
            if (ImageUtils::overlapY(text_box.bbox, fb.bbox, 0.5f)) {
                cv::Rect local = fb.bbox;
                local.x -= text_box.bbox.x;
                local.y -= text_box.bbox.y;
                local &= cv::Rect(0, 0, roi.cols, roi.rows);
                if (local.width > 0 && local.height > 0) {
                    local_formula_rects.push_back(local);
                }
            }
        }

        // 1. 识别公式（批量）
        std::vector<std::string> formula_latex_list;
        std::vector<cv::Mat> formula_crops;
        for (const auto& fr : local_formula_rects) {
            cv::Rect global_fr = fr;
            global_fr.x += text_box.bbox.x;
            global_fr.y += text_box.bbox.y;
            cv::Mat formula_roi = image(global_fr).clone();
            if (formula_roi.empty()) {
                formula_crops.push_back(cv::Mat());
            } else {
                formula_crops.push_back(formula_roi);
            }
        }

        if (!formula_crops.empty() && vlm) {
            formula_latex_list = vlm->batchRecognize(formula_crops, "Table Recognition:", 128);
            for (auto& latex : formula_latex_list) {
                latex = FormatFixer::onlineFormulaFix(latex);
                if (latex.empty()) latex = "$formula$";
            }
        } else {
            for (size_t i = 0; i < local_formula_rects.size(); ++i) {
                formula_latex_list.push_back("$formula$");
            }
        }

        // 2. 填充公式区域（用于 OCR 文本识别）
        if (!local_formula_rects.empty()) {
            ImageUtils::fillFormulaRegions(roi, local_formula_rects, image(text_box.bbox), 3);
        }

        // 3. OCR 识别填充后的 ROI
        std::vector<OCRResult> ocr_results = ocr->recognizeBuffer(roi);
        if (ocr_results.empty()) {
            // 单行情况，按公式框分割文本
            if (!local_formula_rects.empty()) {
                std::vector<int> split_x;
                for (const auto& fr : local_formula_rects) {
                    split_x.push_back(fr.x);
                    split_x.push_back(fr.x + fr.width);
                }
                std::sort(split_x.begin(), split_x.end());
                split_x.erase(std::unique(split_x.begin(), split_x.end()), split_x.end());

                int last_x = 0;
                for (int x : split_x) {
                    if (x > last_x) {
                        cv::Rect text_rect(last_x, 0, x - last_x, roi.rows);
                        text_rect &= cv::Rect(0, 0, roi.cols, roi.rows);
                        if (text_rect.width > 0 && text_rect.height > 0) {
                            cv::Mat text_roi = roi(text_rect).clone();
                            if (!text_roi.empty()) {
                                OCRResult sub_res = ocr->recognizeCrop(text_roi);
                                if (!sub_res.text.empty()) {
                                    sub_res.box = {
                                            {float(text_rect.x), float(text_rect.y)},
                                            {float(text_rect.x + text_rect.width), float(text_rect.y)},
                                            {float(text_rect.x + text_rect.width), float(text_rect.y + text_rect.height)},
                                            {float(text_rect.x), float(text_rect.y + text_rect.height)}
                                    };
                                    ocr_results.push_back(sub_res);
                                }
                            }
                        }
                    }
                    last_x = x;
                }
                if (last_x < roi.cols) {
                    cv::Rect text_rect(last_x, 0, roi.cols - last_x, roi.rows);
                    if (text_rect.width > 0) {
                        cv::Mat text_roi = roi(text_rect).clone();
                        if (!text_roi.empty()) {
                            OCRResult sub_res = ocr->recognizeCrop(text_roi);
                            if (!sub_res.text.empty()) {
                                sub_res.box = {
                                        {float(text_rect.x), float(text_rect.y)},
                                        {float(text_rect.x + text_rect.width), float(text_rect.y)},
                                        {float(text_rect.x + text_rect.width), float(text_rect.y + text_rect.height)},
                                        {float(text_rect.x), float(text_rect.y + text_rect.height)}
                                };
                                ocr_results.push_back(sub_res);
                            }
                        }
                    }
                }
            } else {
                OCRResult ocr_result = ocr->recognizeCrop(roi);
                ocr_result.box = {
                        {0, 0},
                        {float(roi.cols), 0},
                        {float(roi.cols), float(roi.rows)},
                        {0, float(roi.rows)}
                };
                ocr_results.push_back(ocr_result);
            }
        }

        // 4. 构建元素列表
        std::vector<Element> elements;
        for (const auto& res : ocr_results) {
            if (res.text.empty()) continue;
            std::vector<cv::Point2f> pts;
            for (const auto& p : res.box) {
                pts.push_back(cv::Point2f(p[0], p[1]));
            }
            cv::Rect rect = cv::boundingRect(pts);
            rect &= cv::Rect(0, 0, roi.cols, roi.rows);
            if (rect.width > 0 && rect.height > 0) {
                elements.push_back({rect, res.text, false, false});
            }
        }
        for (size_t i = 0; i < local_formula_rects.size(); ++i) {
            elements.push_back({local_formula_rects[i],
                                (i < formula_latex_list.size() ? formula_latex_list[i] : "$formula$"),
                                true, false});
        }

        if (elements.empty()) return "";

        // 按行分组
        std::sort(elements.begin(), elements.end(),
                  [](const Element& a, const Element& b) { return a.bbox.y < b.bbox.y; });

        std::vector<std::vector<Element>> rows;
        for (const auto& el : elements) {
            bool added = false;
            for (auto& row : rows) {
                if (ImageUtils::overlapY(el.bbox, row[0].bbox, 0.8f)) {
                    row.push_back(el);
                    added = true;
                    break;
                }
            }
            if (!added) {
                rows.push_back({el});
            }
        }

        for (auto& row : rows) {
            std::sort(row.begin(), row.end(),
                      [](const Element& a, const Element& b) { return a.bbox.x < b.bbox.x; });
        }

        // 行内合并
        const int MERGE_GAP_THRESHOLD = 30;
        std::string line_text;

        for (auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (row[i].used) continue;

                if (row[i].is_formula) {
                    Element* left_text = nullptr;
                    int left_dist = (std::numeric_limits<int>::max)();
                    for (int j = i - 1; j >= 0; --j) {
                        if (!row[j].is_formula && !row[j].used) {
                            int gap = row[i].bbox.x - (row[j].bbox.x + row[j].bbox.width);
                            if (gap >= 0 && gap < left_dist) {
                                left_dist = gap;
                                left_text = &row[j];
                            }
                            break;
                        }
                    }

                    Element* right_text = nullptr;
                    int right_dist = (std::numeric_limits<int>::max)();
                    for (size_t j = i + 1; j < row.size(); ++j) {
                        if (!row[j].is_formula && !row[j].used) {
                            int gap = row[j].bbox.x - (row[i].bbox.x + row[i].bbox.width);
                            if (gap >= 0 && gap < right_dist) {
                                right_dist = gap;
                                right_text = &row[j];
                            }
                            break;
                        }
                    }

                    bool merge_left = (left_text != nullptr && left_dist < MERGE_GAP_THRESHOLD);
                    bool merge_right = (right_text != nullptr && right_dist < MERGE_GAP_THRESHOLD);

                    if (merge_left && merge_right) {
                        line_text += left_text->text;
                        line_text += row[i].text;
                        line_text += right_text->text;
                        left_text->used = true;
                        right_text->used = true;
                        row[i].used = true;
                    } else if (merge_left) {
                        line_text += left_text->text;
                        line_text += row[i].text;
                        left_text->used = true;
                        row[i].used = true;
                    } else if (merge_right) {
                        line_text += row[i].text;
                        line_text += right_text->text;
                        right_text->used = true;
                        row[i].used = true;
                    } else {
                        line_text += row[i].text;
                        row[i].used = true;
                    }
                } else {
                    if (!row[i].used) {
                        line_text += row[i].text;
                        row[i].used = true;
                    }
                }
            }
        }

        return line_text;
    }

// ---------- 主处理流程 ----------
    std::vector<std::string> DocumentProcessor::process(const cv::Mat& image, float score_threshold) {
        if (image.empty()) {
            std::cerr << "Input image is empty!" << std::endl;
            return {};
        }

        // 确保所需引擎已加载（支持懒加载）
        ctx_.ensureLayoutDetector();
        ctx_.ensureOCREngine();
        ctx_.ensureVLMEngine();

        // 获取所需引擎指针
        auto* layout = ctx_.getLayoutDetector();
        auto* ocr = ctx_.getOCREngine();
        auto* vlm = ctx_.getVLMEngine();

        if (!layout || !ocr) {
            std::cerr << "Layout or OCR engine not available." << std::endl;
            return {};
        }

        // -------- 图像预处理（去扭曲、增强） --------
        cv::Mat processed_image = image;
        if (config_.enable_warp || config_.enable_enhance) {
            ctx_.ensureDocProc();
        }
        void* docproc = ctx_.getDocProcHandle();
        if (docproc && (config_.enable_warp || config_.enable_enhance)) {
            auto* dll_loader = ctx_.getDLLLoader();
            if (dll_loader && dll_loader->docproc_process) {
                unsigned char* output = nullptr;
                int outWidth = 0, outHeight = 0, outChannels = 0;
                bool success = dll_loader->docproc_process(
                        docproc,
                        const_cast<unsigned char*>(image.data),
                        image.cols, image.rows, image.channels(),
                        config_.enable_warp,
                        config_.enable_enhance,
                        &output,
                        &outWidth, &outHeight, &outChannels
                );

                if (success && output && outWidth > 0 && outHeight > 0 && outChannels > 0) {
                    cv::Mat temp;
                    if (outChannels == 3) {
                        temp = cv::Mat(outHeight, outWidth, CV_8UC3, output);
                    } else if (outChannels == 1) {
                        temp = cv::Mat(outHeight, outWidth, CV_8UC1, output);
                        cv::cvtColor(temp, temp, cv::COLOR_GRAY2BGR);
                    } else if (outChannels == 4) {
                        temp = cv::Mat(outHeight, outWidth, CV_8UC4, output);
                        cv::cvtColor(temp, temp, cv::COLOR_BGRA2BGR);
                    } else {
                        std::cerr << "Unsupported output channels: " << outChannels << std::endl;
                        dll_loader->docproc_free_output(output);
                        processed_image = image;
                        goto after_preprocess;
                    }

                    if (!temp.empty()) {
                        processed_image = temp.clone();
                        dll_loader->docproc_free_output(output);
                        std::cout << "Image preprocessed (warp=" << config_.enable_warp
                                  << ", enhance=" << config_.enable_enhance << ")" << std::endl;
                    } else {
                        dll_loader->docproc_free_output(output);
                        processed_image = image;
                    }
                } else {
                    std::cerr << "DocumentImageProcessor processing failed, using original." << std::endl;
                    processed_image = image;
                }
            }
        }

        after_preprocess:

        // 1. 版面检测
        auto boxes = layout->detect(processed_image, score_threshold);
        if (boxes.empty()) {
            std::cerr << "No layout boxes detected." << std::endl;
            return {};
        }

        // 2. 提取公式框（用于文本块内联公式）
        std::vector<BoxInfo> formula_boxes;
        for (const auto& b : boxes) {
            if (b.label == "formula" || b.label == "inline_formula") {
                formula_boxes.push_back(b);
            }
        }

        // 3. 按顺序处理每个框
        std::vector<std::string> output_segments;

        for (const auto& box : boxes) {
            const auto& label = box.label;
            std::string content;

            // ---------- 文本类 ----------
            if (isTextType(label)) {
                content = processTextBlock(processed_image, box, formula_boxes);
                if (!content.empty()) {
                    output_segments.push_back(content);
                }
            }
                // ---------- 标题类 ----------
            else if (isTitleType(label)) {
                cv::Mat roi = processed_image(box.bbox).clone();
                auto res = ocr->recognizeCrop(roi);
                std::string title = res.text.empty() ? (label == "doc_title" ? "[标题]" : "[段落标题]") : res.text;
                if (label == "doc_title") {
                    output_segments.push_back("# " + title);
                } else {
                    output_segments.push_back("## " + title);
                }
            }
            else if (label == "figure_title") {
                auto res = ocr->recognizeCrop(processed_image(box.bbox).clone());
                std::string title = res.text.empty() ? "[图标题]" : res.text;
                output_segments.push_back("<p align=\"center\">" + title + "</p>");
            }
                // ---------- 页眉/页脚 ----------
            else if (label == "header" || label == "footer") {
                auto res = ocr->recognizeCrop(processed_image(box.bbox).clone());
                std::string text = res.text;
                if (!text.empty()) {
                    output_segments.push_back(text);
                }
            }
                // ---------- 图片/图表/印章 ----------
            else if (label == "image" || label == "chart" || label == "seal" ||
                     label == "header_image" || label == "footer_image") {
                cv::Mat roi = processed_image(box.bbox);
                if (!roi.empty()) {
                    std::string img_path = ImageUtils::saveImage(roi, label, config_.assets_dir, image_counter_);
                    output_segments.push_back("![" + label + "](" + img_path + ")");
                } else {
                    output_segments.push_back("![图片]()");
                }
            }
                // ---------- 表格 ----------
            else if (label == "table") {
                if (vlm) {
                    cv::Mat roi = processed_image(box.bbox);
                    if (!roi.empty()) {
                        std::string table_md = vlm->recognize(roi, "Table Recognition:", 512);
                        table_md = FormatFixer::toMarkdownTable(table_md);
                        if (!table_md.empty()) {
                            output_segments.push_back(table_md);
                        } else {
                            output_segments.push_back("[表格]");
                        }
                    } else {
                        output_segments.push_back("[表格]");
                    }
                } else {
                    output_segments.push_back("[表格（VLM未加载）]");
                }
            }
                // ---------- 公式编号 ----------
            else if (label == "formula_number") {
                auto res = ocr->recognizeCrop(processed_image(box.bbox).clone());
                if (!res.text.empty()) {
                    output_segments.push_back("(" + res.text + ")");
                }
            }
                // ---------- 参考文献引用 ----------
            else if (label == "reference") {
                auto res = ocr->recognizeCrop(processed_image(box.bbox).clone());
                if (!res.text.empty()) {
                    output_segments.push_back("[" + res.text + "]");
                } else {
                    output_segments.push_back("[参考文献]");
                }
            }
                // ---------- 编号 ----------
            else if (label == "number") {
                auto res = ocr->recognizeCrop(processed_image(box.bbox).clone());
                if (!res.text.empty()) {
                    output_segments.push_back(res.text);
                }
            }
                // ---------- 脚注 ----------
            else if (label == "footnote" || label == "vision_footnote") {
                auto res = ocr->recognizeCrop(processed_image(box.bbox).clone());
                if (!res.text.empty()) {
                    output_segments.push_back("[^" + res.text + "]");
                }
            }
                // ---------- 独立公式（未被文本包含） ----------
            else if (label == "formula" || label == "inline_formula") {
                bool contained = false;
                for (const auto& tb : boxes) {
                    if (isTextType(tb.label)) {
                        if (ImageUtils::overlapY(tb.bbox, box.bbox, 0.5f)) {
                            contained = true;
                            break;
                        }
                    }
                }
                if (!contained) {
                    std::string latex = recognizeFormula(processed_image(box.bbox).clone());
                    if (!latex.empty()) {
                        output_segments.push_back("$$" + latex + "$$");
                    } else {
                        output_segments.push_back("$$formula$$");
                    }
                }
            }
                // ---------- 其他：尝试 OCR ----------
            else {
                auto res = ocr->recognizeCrop(processed_image(box.bbox).clone());
                if (!res.text.empty()) {
                    output_segments.push_back(res.text);
                } else {
                    output_segments.push_back("[" + label + "]");
                }
            }
        }

        return output_segments;
    }

} // namespace docmind