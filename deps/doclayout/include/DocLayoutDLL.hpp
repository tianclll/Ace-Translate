#pragma once

#include "export.hpp"

extern "C"
{

struct LayoutResult
{
    int class_id;

    float score;

    int x;
    int y;
    int width;
    int height;

    char label[64];
};

DOCLAYOUT_API void* CreatePPDocLayout();

DOCLAYOUT_API bool InitPPDocLayout(
        void* handle,
        const char* model_path,
        bool use_gpu = true,
        int device_id = 0
);

DOCLAYOUT_API int DetectLayout(void* handle,const unsigned char* data, int width, int height, int channels,LayoutResult* results,int max_count);

DOCLAYOUT_API void DestroyPPDocLayout(
        void* handle
);

}