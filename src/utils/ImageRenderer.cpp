#include "docmind/utils/ImageRenderer.hpp"
#include "docmind/utils/FontManager.hpp"
#include <vector>
#include <algorithm>
#include <iostream>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _WIN32
#include <windows.h>
#include <wingdi.h>
#endif
#include <opencv2/photo.hpp>

namespace docmind {

    struct ImageRenderer::Impl {
        FontManager* fontMgr;
        cv::Scalar textColor = cv::Scalar(0,0,0);
        cv::Scalar bgColor = cv::Scalar(255,255,255);

        Impl(FontManager* mgr) : fontMgr(mgr) {}

        void eraseText(cv::Mat& img, const std::vector<cv::Point2f>& box) {
            if (box.size() < 4) return;
            cv::Rect rect = cv::boundingRect(box);
            int expand = 2;
            rect.x = std::max(0, rect.x - expand);
            rect.y = std::max(0, rect.y - expand);
            rect.width = std::min(img.cols - rect.x, rect.width + 2 * expand);
            rect.height = std::min(img.rows - rect.y, rect.height + 2 * expand);
            if (rect.width <= 0 || rect.height <= 0) return;
            cv::Mat mask = cv::Mat::zeros(img.size(), CV_8U);
            cv::rectangle(mask, rect, cv::Scalar(255), cv::FILLED);
            cv::inpaint(img, mask, img, 5, cv::INPAINT_TELEA);
        }

        void eraseRect(cv::Mat& img, const cv::Rect& rect) {
            cv::Rect r = rect;
            int expand = 2;
            r.x = std::max(0, r.x - expand);
            r.y = std::max(0, r.y - expand);
            r.width = std::min(img.cols - r.x, r.width + 2 * expand);
            r.height = std::min(img.rows - r.y, r.height + 2 * expand);
            if (r.width <= 0 || r.height <= 0) return;
            cv::Mat mask = cv::Mat::zeros(img.size(), CV_8U);
            cv::rectangle(mask, r, cv::Scalar(255), cv::FILLED);
            cv::inpaint(img, mask, img, 5, cv::INPAINT_TELEA);
        }

        bool hasNonASCII(const std::string& str) {
            for (unsigned char c : str) {
                if (c > 0x7F) return true;
            }
            return false;
        }

        // 纯 C 函数，用 __try/__except 保护 GDI 调用（不含 C++ 对象）
        static bool gdiDrawTextOnBitmap(cv::Mat& img,
                                         const wchar_t* wtext,
                                         int fontSize,
                                         int maxWidth, int maxHeight,
                                         int startX, int startY,
                                         int copyW, int copyH,
                                         cv::Scalar textColor)
        {
            bool success = false;
            __try {
                HDC hdc = GetDC(NULL);
                if (!hdc) return false;

                HDC memDC = CreateCompatibleDC(hdc);
                if (!memDC) { ReleaseDC(NULL, hdc); return false; }

                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = maxWidth;
                bmi.bmiHeader.biHeight = -maxHeight;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 24;
                bmi.bmiHeader.biCompression = BI_RGB;
                void* bits = nullptr;
                HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
                if (!hBitmap) {
                    DeleteDC(memDC);
                    ReleaseDC(NULL, hdc);
                    return false;
                }
                HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBitmap);

                int rowBytes = ((maxWidth * 24 + 31) / 32) * 4;
                for (int y = 0; y < copyH; ++y) {
                    BYTE* src = img.ptr<BYTE>(startY + y) + startX * 3;
                    BYTE* dst = (BYTE*)bits + y * rowBytes;
                    memcpy(dst, src, copyW * 3);
                }

                SetBkMode(memDC, TRANSPARENT);
                SetTextColor(memDC, RGB(textColor[2], textColor[1], textColor[0]));

                const wchar_t* fontNames[] = { L"Microsoft YaHei", L"SimHei", L"SimSun", L"Arial" };
                HFONT hFont = nullptr;
                for (const wchar_t* name : fontNames) {
                    hFont = CreateFontW(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, name);
                    if (hFont) break;
                }
                if (!hFont) hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                SelectObject(memDC, hFont);

                RECT rect = { 0, 0, maxWidth, maxHeight };
                DrawTextW(memDC, wtext, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                for (int y = 0; y < copyH; ++y) {
                    BYTE* src = (BYTE*)bits + y * rowBytes;
                    BYTE* dst = img.ptr<BYTE>(startY + y) + startX * 3;
                    memcpy(dst, src, copyW * 3);
                }

                SelectObject(memDC, oldBmp);
                DeleteObject(hFont);
                DeleteDC(memDC);
                DeleteObject(hBitmap);
                ReleaseDC(NULL, hdc);
                success = true;
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                success = false;
            }
            return success;
        }

        void drawTextGDI(cv::Mat& img, const std::string& text, const FontInfo& info) {
#ifdef _WIN32
            int maxWidth = info.maxWidth;
            int maxHeight = info.maxHeight;
            if (maxWidth <= 0 || maxHeight <= 0) return;

            int fontSize = std::max(8, static_cast<int>(maxHeight * 0.9));

            // UTF-8 → UTF-16
            int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            if (wlen <= 0) return;
            std::wstring wtext(wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wtext[0], wlen);
            wtext.resize(wlen - 1);

            int startX = info.position.x;
            int startY = info.position.y;
            int copyW = std::min(maxWidth, img.cols - startX);
            int copyH = std::min(maxHeight, img.rows - startY);
            if (copyW <= 0 || copyH <= 0) return;

            if (!gdiDrawTextOnBitmap(img, wtext.c_str(), fontSize,
                                      maxWidth, maxHeight, startX, startY,
                                      copyW, copyH, textColor)) {
                // GDI 失败（工作线程），回退到 OpenCV putText
                if (maxWidth > 4 && maxHeight > 4 && !text.empty()) {
                    int baseline = 0;
                    double scale = std::min(1.0, std::min(maxWidth / 80.0, maxHeight / 20.0));
                    scale = std::max(0.3, std::min(scale, 3.0));
                    int cx = startX + maxWidth / 2;
                    int cy = startY + maxHeight / 2 + static_cast<int>(scale * 4);
                    cx = std::max(0, std::min(cx, img.cols - 1));
                    cy = std::max(0, std::min(cy, img.rows - 1));
                    cv::putText(img, text, cv::Point(cx - maxWidth/4, cy),
                                cv::FONT_HERSHEY_SIMPLEX, scale, textColor, 1, cv::LINE_AA);
                }
            }
#endif
        }

        void drawText(cv::Mat& img, const std::string& text, const FontInfo& info) {
            if (text.empty()) return;
            if (hasNonASCII(text)) {
                drawTextGDI(img, text, info);
                return;
            }
            int maxWidth = info.maxWidth;
            int maxHeight = info.maxHeight;
            if (maxWidth <= 0 || maxHeight <= 0) return;
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 1.0, 1, &baseline);
            if (textSize.width == 0 || textSize.height == 0) return;
            double scaleX = (double)(maxWidth - 2) / textSize.width;
            double scaleY = (double)(maxHeight - 2) / textSize.height;
            double scale = std::min(scaleX, scaleY);
            scale = std::max(0.3, std::min(scale, 3.0));
            textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, scale, 1, &baseline);
            int x = info.position.x + (maxWidth - textSize.width) / 2;
            int y = info.position.y + (maxHeight - textSize.height) / 2 + baseline;
            x = std::max(0, std::min(x, img.cols - 1));
            y = std::max(0, std::min(y, img.rows - 1));
            cv::putText(img, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, scale, textColor, 1, cv::LINE_AA);
        }
    };

    ImageRenderer::ImageRenderer(FontManager* fontMgr)
            : pImpl(std::make_unique<Impl>(fontMgr)) {}

    ImageRenderer::~ImageRenderer() = default;

    void ImageRenderer::eraseText(cv::Mat& img, const std::vector<cv::Point2f>& box) {
        pImpl->eraseText(img, box);
    }

    void ImageRenderer::eraseRect(cv::Mat& img, const cv::Rect& rect) {
        pImpl->eraseRect(img, rect);
    }

    void ImageRenderer::drawText(cv::Mat& img, const std::string& text, const FontInfo& info) {
        pImpl->drawText(img, text, info);
    }

    void ImageRenderer::setTextColor(const cv::Scalar& color) {
        pImpl->textColor = color;
    }

    void ImageRenderer::setBackgroundColor(const cv::Scalar& color) {
        pImpl->bgColor = color;
    }

} // namespace docmind