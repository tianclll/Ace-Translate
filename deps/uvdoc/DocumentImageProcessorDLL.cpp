#include "DocumentImageProcessorDLL.hpp"

#include "DocumentImageProcessor.hpp"

#include <opencv2/opencv.hpp>

#include <cstring>



void* CreateProcessor()
{
    return new DocumentImageProcessor();
}



bool InitProcessor(
        void* handle,
        const char* model,
        bool useCUDA,
        int gpuID)
{

    if(!handle)
        return false;


    auto ptr =
            static_cast<DocumentImageProcessor*>(handle);


    return ptr->Init(
            model,
            useCUDA,
            gpuID);
}




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
        int* outChannels)
{

    if(!handle || !data)
        return false;



    auto ptr =
            static_cast<DocumentImageProcessor*>(handle);



    cv::Mat img;


    if(channels==3)
    {
        img=cv::Mat(
                height,
                width,
                CV_8UC3,
                data);
    }
    else
    {
        img=cv::Mat(
                height,
                width,
                CV_8UC1,
                data);
    }



    cv::Mat result =
            ptr->Process(
                    img,
                    warp,
                    enhance);



    if(result.empty())
        return false;



    int size =
            result.total()
            *
            result.elemSize();



    unsigned char* buffer =
            new unsigned char[size];


    memcpy(
            buffer,
            result.data,
            size);



    *output=buffer;

    *outWidth=result.cols;

    *outHeight=result.rows;

    *outChannels=result.channels();


    return true;

}




void ReleaseProcessor(
        void* handle)
{

    if(handle)
    {
        delete
                static_cast<DocumentImageProcessor*>(handle);
    }

}




void FreeOutput(
        unsigned char* data)
{

    delete[] data;

}