#pragma once

#ifdef _WIN32
#ifdef DOCLAYOUT_EXPORTS
#define DOCLAYOUT_API __declspec(dllexport)
#else
#define DOCLAYOUT_API __declspec(dllimport)
#endif
#else
#define DOCLAYOUT_API
#endif