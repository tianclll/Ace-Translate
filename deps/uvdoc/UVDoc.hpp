#pragma once

#include <string>
#include <vector>
#include <memory>

#include <opencv2/opencv.hpp>

#include <onnxruntime_cxx_api.h>

class UVDoc
{
public:
    UVDoc();
    ~UVDoc();

    // 初始化模型
    bool Init(const std::string& modelPath,
              bool useCUDA = false,
              int gpuID = 0);

    // 图片推理
    cv::Mat Predict(
            const cv::Mat& image);

private:

    //-------------------------
    // 前处理
    //-------------------------
    bool Preprocess(
            const cv::Mat& image,
            std::vector<float>& inputTensorValues,
            cv::Mat& originalRGB,
            int& originalWidth,
            int& originalHeight);

    //-------------------------
    // 后处理
    //-------------------------
    cv::Mat Postprocess(
            const float* gridData,
            int gridH,
            int gridW,
            const cv::Mat& originalRGB,
            int originalWidth,
            int originalHeight);

    //-------------------------
    // Normalize
    //-------------------------
    inline float Normalize(
            float value,
            float mean,
            float std);

private:

    Ort::Env mEnv;

    Ort::SessionOptions mOptions;

    std::unique_ptr<Ort::Session> mSession;

    std::string mInputName;
    std::string mOutputName;

    std::vector<int64_t> mInputShape;
    std::vector<int64_t> mOutputShape;

    int mInputWidth = 0;
    int mInputHeight = 0;

    bool mUseCUDA = false;
    int mGpuID = 0;
    //-------------------------
    // ImageNet Normalize
    //-------------------------
    const float mMean[3] =
            {
                    0.485f,
                    0.456f,
                    0.406f
            };

    const float mStd[3] =
            {
                    0.229f,
                    0.224f,
                    0.225f
            };
};
