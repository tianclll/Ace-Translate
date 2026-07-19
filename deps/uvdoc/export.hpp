#pragma once


#ifdef DOCPROC_EXPORTS

#define DOCPROC_API __declspec(dllexport)

#else

#define DOCPROC_API __declspec(dllimport)

#endif