#pragma once

#include <opencv2/opencv.hpp>
#include <string>

#include "UVDoc.hpp"
#include "ImageEnhancer.hpp"



class DocumentImageProcessor
{

public:

    DocumentImageProcessor();


    ~DocumentImageProcessor();



    /*
        初始化

        uvdocModel:
            UVDoc ONNX模型

        useCUDA:
            是否使用GPU

        gpuID:
            GPU编号
    */
    bool Init(
            const std::string& uvdocModel,
            bool useCUDA = true,
            int gpuID = 0);



    /*
        文档图片处理


        image:
            输入图片(BGR)


        enableWarp:
            是否开启UVDoc矫正


        enableEnhance:
            是否开启扫描增强


        return:
            处理后的图片
    */
    cv::Mat Process(
            const cv::Mat& image,
            bool enableWarp = true,
            bool enableEnhance = false);



    /*
        单独执行UVDoc
    */
    cv::Mat Warp(
            const cv::Mat& image);



    /*
        单独执行增强
    */
    cv::Mat Enhance(
            const cv::Mat& image);



private:


    UVDoc mUVDoc;


    ImageEnhancer mEnhancer;


    bool mInitialized;

};