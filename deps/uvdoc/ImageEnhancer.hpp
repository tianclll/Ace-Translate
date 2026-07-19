#pragma once

#include <opencv2/opencv.hpp>


class ImageEnhancer
{

public:

    ImageEnhancer();


    // 主增强接口
    cv::Mat Enhance(
            const cv::Mat& image);


private:


    // Gamma
    cv::Mat GammaCorrection(
            const cv::Mat& src,
            float gamma);


    // 光照均衡
    cv::Mat IlluminationNormalize(
            const cv::Mat& src);

};