// include/export.hpp
#ifndef OCR_EXPORT_HPP
#define OCR_EXPORT_HPP

#ifdef _WIN32
#ifdef OCR_EXPORTS
#define OCR_API __declspec(dllexport)
#else
#define OCR_API __declspec(dllimport)
#endif
#else
#define OCR_API __attribute__((visibility("default")))
#endif

#endif // OCR_EXPORT_HPP