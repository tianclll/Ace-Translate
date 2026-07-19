#pragma once

#include "export.hpp"


extern "C"
{


DOCPROC_API
void* CreateProcessor();



DOCPROC_API
bool InitProcessor(
        void* handle,
        const char* model,
        bool useCUDA,
        int gpuID);



DOCPROC_API
bool ProcessImage(
        void* handle,

        unsigned char* data,

        int width,
        int height,
        int channels,

        bool warp,
        bool enhance,


        unsigned char** output,

        int* outWidth,
        int* outHeight,
        int* outChannels);



DOCPROC_API
void ReleaseProcessor(
        void* handle);



DOCPROC_API
void FreeOutput(
        unsigned char* data);



}