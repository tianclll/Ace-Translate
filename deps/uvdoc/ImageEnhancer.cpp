#include "ImageEnhancer.hpp"

#include <vector>


ImageEnhancer::ImageEnhancer()
{

}



//----------------------------------------------------
// Gamma Correction
//----------------------------------------------------
cv::Mat ImageEnhancer::GammaCorrection(
        const cv::Mat& src,
        float gamma)
{

    cv::Mat lut(1,256,CV_8UC1);


    uchar* p = lut.ptr();


    for(int i=0;i<256;i++)
    {
        p[i] =
                cv::saturate_cast<uchar>(
                        pow(i/255.0,gamma)
                        *
                        255.0);
    }



    cv::Mat dst;


    cv::LUT(
            src,
            lut,
            dst);


    return dst;
}





//----------------------------------------------------
// Background illumination normalize
//----------------------------------------------------
cv::Mat ImageEnhancer::IlluminationNormalize(
        const cv::Mat& src)
{

    cv::Mat floatImg;


    src.convertTo(
            floatImg,
            CV_32FC1,
            1.0/255.0);



    //-----------------------------------------------
    // Gaussian background
    //-----------------------------------------------

    cv::Mat gauss;


    cv::GaussianBlur(
            floatImg,
            gauss,
            cv::Size(101,101),
            0);



    //-----------------------------------------------
    // src / background
    //-----------------------------------------------

    cv::Mat dst =
            floatImg / gauss;



    cv::threshold(
            dst,
            dst,
            2.0,
            2.0,
            cv::THRESH_TRUNC);



    dst *= 255.0;



    dst.convertTo(
            dst,
            CV_8UC1);



    return dst;
}





//----------------------------------------------------
// Enhance
//----------------------------------------------------
cv::Mat ImageEnhancer::Enhance(
        const cv::Mat& image)
{

    cv::Mat gray;


    //-----------------------------------------------
    // BGR -> Gray
    //-----------------------------------------------
    if(image.channels()==3)
    {
        cv::cvtColor(
                image,
                gray,
                cv::COLOR_BGR2GRAY);
    }
    else
    {
        gray=image.clone();
    }



    //-----------------------------------------------
    // illumination
    //-----------------------------------------------

    cv::Mat dst =
            IlluminationNormalize(gray);



    //-----------------------------------------------
    // Gamma
    //-----------------------------------------------

    dst =
            GammaCorrection(
                    dst,
                    1.5);



    return dst;
}