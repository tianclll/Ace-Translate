#include "DocumentImageProcessor.hpp"

#include <iostream>



DocumentImageProcessor::DocumentImageProcessor()
{
    mInitialized = false;
}



DocumentImageProcessor::~DocumentImageProcessor()
{

}





bool DocumentImageProcessor::Init(
        const std::string& uvdocModel,
        bool useCUDA,
        int gpuID)
{


    if(!mUVDoc.Init(
            uvdocModel,
            useCUDA,
            gpuID))
    {

        std::cerr
                << "[DocumentImageProcessor] UVDoc Init Failed!"
                << std::endl;


        return false;
    }



    mInitialized = true;



    std::cout
            << "[DocumentImageProcessor] Ready"
            << std::endl;



    return true;

}





cv::Mat DocumentImageProcessor::Warp(
        const cv::Mat& image)
{

    if(image.empty())
    {
        return cv::Mat();
    }



    return mUVDoc.Predict(image);

}





cv::Mat DocumentImageProcessor::Enhance(
        const cv::Mat& image)
{

    if(image.empty())
    {
        return cv::Mat();
    }



    return mEnhancer.Enhance(image);

}






cv::Mat DocumentImageProcessor::Process(
        const cv::Mat& image,
        bool enableWarp,
        bool enableEnhance)
{

    if(!mInitialized)
    {

        std::cerr
                << "[DocumentImageProcessor] Not initialized!"
                << std::endl;


        return cv::Mat();
    }



    if(image.empty())
    {

        std::cerr
                << "Input image empty!"
                << std::endl;


        return cv::Mat();
    }




    cv::Mat result;



    //------------------------------------------------
    // Step 1: UVDoc
    //------------------------------------------------

    if(enableWarp)
    {

        result =
                Warp(image);


        if(result.empty())
        {

            std::cerr
                    << "Warp failed!"
                    << std::endl;


            return cv::Mat();

        }

    }
    else
    {

        result =
                image.clone();

    }




    //------------------------------------------------
    // Step 2: Enhancement
    //------------------------------------------------

    if(enableEnhance)
    {

        result =
                Enhance(result);


        if(result.empty())
        {

            std::cerr
                    << "Enhance failed!"
                    << std::endl;


            return cv::Mat();

        }

    }



    return result;

}