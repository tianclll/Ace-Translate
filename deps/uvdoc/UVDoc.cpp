#include "UVDoc.hpp"

#include <iostream>

#ifdef USE_CUDA
#include <cuda_provider_factory.h>
#endif

//------------------------------------------------------------
// Constructor
//------------------------------------------------------------
UVDoc::UVDoc()
        : mEnv(ORT_LOGGING_LEVEL_WARNING, "UVDoc")
{
}

//------------------------------------------------------------
// Destructor
//------------------------------------------------------------
UVDoc::~UVDoc()
{
    if (mSession)
    {
        mSession.reset();
    }
}

//------------------------------------------------------------
// Init
//------------------------------------------------------------
bool UVDoc::Init(
        const std::string& modelPath,
        bool useCUDA,
        int gpuID)
{
    try
    {
        auto providers = Ort::GetAvailableProviders();

        std::cout << "Available Providers:" << std::endl;

        for (const auto& p : providers)
        {
            std::cout << "  " << p << std::endl;
        }
        //--------------------------------------------------------
        // Save Config
        //--------------------------------------------------------
        mUseCUDA = useCUDA;
        mGpuID = gpuID;

        //--------------------------------------------------------
        // Session Options
        //--------------------------------------------------------
        mOptions = Ort::SessionOptions();

        mOptions.SetGraphOptimizationLevel(
                GraphOptimizationLevel::ORT_ENABLE_ALL);

        mOptions.SetExecutionMode(
                ExecutionMode::ORT_SEQUENTIAL);

        mOptions.SetIntraOpNumThreads(4);

        mOptions.SetInterOpNumThreads(1);



        if (mUseCUDA)
        {
            OrtCUDAProviderOptions cudaOptions{};

            cudaOptions.device_id = gpuID;

            cudaOptions.arena_extend_strategy =
                    0;

            cudaOptions.cudnn_conv_algo_search =
                    OrtCudnnConvAlgoSearchExhaustive;

            cudaOptions.do_copy_in_default_stream =
                    1;


            mOptions.AppendExecutionProvider_CUDA(
                    cudaOptions);



            std::cout
                << "[UVDoc] CUDA Enabled"
                << std::endl;
        }


        else{
            std::cout
                    << "[UVDoc] CPU Mode"
                    << std::endl;
        }

        //--------------------------------------------------------
        // Windows Unicode Path
        //--------------------------------------------------------
#ifdef _WIN32

        std::wstring modelPathW(
                modelPath.begin(),
                modelPath.end());

        mSession = std::make_unique<Ort::Session>(
                mEnv,
                modelPathW.c_str(),
                mOptions);

#else

        mSession = std::make_unique<Ort::Session>(
            mEnv,
            modelPath.c_str(),
            mOptions);

#endif

        //--------------------------------------------------------
        // Allocator
        //--------------------------------------------------------
        Ort::AllocatorWithDefaultOptions allocator;

        //--------------------------------------------------------
        // Input Name
        //--------------------------------------------------------
        {
            auto inputName =
                    mSession->GetInputNameAllocated(
                            0,
                            allocator);

            mInputName =
                    inputName.get();

            auto tensorInfo =
                    mSession
                            ->GetInputTypeInfo(0)
                            .GetTensorTypeAndShapeInfo();

            mInputShape =
                    tensorInfo.GetShape();
        }

        //--------------------------------------------------------
        // Output Name
        //--------------------------------------------------------
        {
            auto outputName =
                    mSession->GetOutputNameAllocated(
                            0,
                            allocator);

            mOutputName =
                    outputName.get();

            auto tensorInfo =
                    mSession
                            ->GetOutputTypeInfo(0)
                            .GetTensorTypeAndShapeInfo();

            mOutputShape =
                    tensorInfo.GetShape();
        }

        //--------------------------------------------------------
        // Input Size
        //--------------------------------------------------------
        if (mInputShape.size() != 4)
        {
            std::cerr
                    << "Invalid Input Shape."
                    << std::endl;

            return false;
        }

//--------------------------------------------------------
// UVDoc Fixed Input Size
//--------------------------------------------------------
        mInputWidth = 672;
        mInputHeight = 896;

        //--------------------------------------------------------
        // Print
        //--------------------------------------------------------
        std::cout << std::endl;
        std::cout << "========== UVDoc ==========" << std::endl;

        std::cout
                << "Input Name : "
                << mInputName
                << std::endl;

        std::cout
                << "Output Name : "
                << mOutputName
                << std::endl;

        std::cout
                << "Input Size : "
                << mInputWidth
                << " x "
                << mInputHeight
                << std::endl;

        std::cout
                << "Provider : "
                << (mUseCUDA ? "CUDA" : "CPU")
                << std::endl;

        std::cout
                << "==========================="
                << std::endl;

        return true;
    }
    catch (const Ort::Exception& e)
    {
        std::cerr
                << "[ONNXRuntime] "
                << e.what()
                << std::endl;

        return false;
    }
}
//------------------------------------------------------------
// Normalize
//------------------------------------------------------------
inline float UVDoc::Normalize(
        float value,
        float mean,
        float std)
{
    return (value - mean) / std;
}

//------------------------------------------------------------
// Preprocess
//------------------------------------------------------------
bool UVDoc::Preprocess(
        const cv::Mat& image,
        std::vector<float>& inputTensorValues,
        cv::Mat& originalRGB,
        int& originalWidth,
        int& originalHeight)
{
    //--------------------------------------------------------
    // Check Image
    //--------------------------------------------------------
    if (image.empty())
    {
        std::cerr
                << "[UVDoc] Input image is empty."
                << std::endl;

        return false;
    }

    //--------------------------------------------------------
    // BGR -> RGB
    //--------------------------------------------------------
    switch (image.channels())
    {
        case 1:
            cv::cvtColor(
                    image,
                    originalRGB,
                    cv::COLOR_GRAY2RGB);
            break;

        case 3:
            cv::cvtColor(
                    image,
                    originalRGB,
                    cv::COLOR_BGR2RGB);
            break;

        case 4:
            cv::cvtColor(
                    image,
                    originalRGB,
                    cv::COLOR_BGRA2RGB);
            break;

        default:

            std::cerr
                    << "[UVDoc] Unsupported image format."
                    << std::endl;

            return false;
    }

    //--------------------------------------------------------
    // Original Size
    //--------------------------------------------------------
    originalWidth = originalRGB.cols;

    originalHeight = originalRGB.rows;

    //--------------------------------------------------------
    // Resize
    //--------------------------------------------------------
    cv::Mat resized;

    cv::resize(
            originalRGB,
            resized,
            cv::Size(
                    mInputWidth,
                    mInputHeight),
            0,
            0,
            cv::INTER_LINEAR);

    //--------------------------------------------------------
    // uint8 -> float32
    //--------------------------------------------------------
    resized.convertTo(
            resized,
            CV_32FC3,
            1.0 / 255.0);

    //--------------------------------------------------------
    // Allocate Tensor
    //--------------------------------------------------------
    inputTensorValues.resize(
            3 *
            mInputWidth *
            mInputHeight);

    const int channelSize =
            mInputWidth *
            mInputHeight;

    //--------------------------------------------------------
    // HWC -> CHW
    //--------------------------------------------------------
    for (int y = 0; y < mInputHeight; y++)
    {
        const cv::Vec3f* row =
                resized.ptr<cv::Vec3f>(y);

        for (int x = 0; x < mInputWidth; x++)
        {
            const cv::Vec3f& pixel =
                    row[x];

            inputTensorValues[
                    y * mInputWidth + x] =
                    Normalize(
                            pixel[0],
                            mMean[0],
                            mStd[0]);

            inputTensorValues[
                    channelSize +
                    y * mInputWidth + x] =
                    Normalize(
                            pixel[1],
                            mMean[1],
                            mStd[1]);

            inputTensorValues[
                    channelSize * 2 +
                    y * mInputWidth + x] =
                    Normalize(
                            pixel[2],
                            mMean[2],
                            mStd[2]);
        }
    }

    return true;
}
//------------------------------------------------------------
// Predict
//------------------------------------------------------------
cv::Mat UVDoc::Predict(const cv::Mat& image)
{
    //--------------------------------------------------------
    // Preprocess
    //--------------------------------------------------------
    std::vector<float> inputTensorValues;

    cv::Mat originalRGB;

    int originalWidth = 0;
    int originalHeight = 0;

    if (!Preprocess(
            image,
            inputTensorValues,
            originalRGB,
            originalWidth,
            originalHeight))
    {
        return cv::Mat();
    }

    //--------------------------------------------------------
    // Input Shape
    //--------------------------------------------------------
    std::array<int64_t, 4> inputShape =
            {
                    1,
                    3,
                    mInputHeight,
                    mInputWidth
            };

    //--------------------------------------------------------
    // Create MemoryInfo
    //--------------------------------------------------------
    Ort::MemoryInfo memoryInfo =
            Ort::MemoryInfo::CreateCpu(
                    OrtArenaAllocator,
                    OrtMemTypeDefault);

    //--------------------------------------------------------
    // Create Tensor
    //--------------------------------------------------------
    Ort::Value inputTensor =
            Ort::Value::CreateTensor<float>(
                    memoryInfo,
                    inputTensorValues.data(),
                    inputTensorValues.size(),
                    inputShape.data(),
                    inputShape.size());

    //--------------------------------------------------------
    // Input / Output Name
    //--------------------------------------------------------
    const char* inputNames[] =
            {
                    mInputName.c_str()
            };

    const char* outputNames[] =
            {
                    mOutputName.c_str()
            };

    //--------------------------------------------------------
    // Run
    //--------------------------------------------------------
    auto outputTensors =
            mSession->Run(
                    Ort::RunOptions{nullptr},
                    inputNames,
                    &inputTensor,
                    1,
                    outputNames,
                    1);

    //--------------------------------------------------------
    // Check Output
    //--------------------------------------------------------
    if (outputTensors.empty())
    {
        std::cerr
                << "[UVDoc] Empty Output."
                << std::endl;

        return cv::Mat();
    }

    //--------------------------------------------------------
    // Output Tensor
    //--------------------------------------------------------
    Ort::Value& outputTensor =
            outputTensors.front();

    if (!outputTensor.IsTensor())
    {
        std::cerr
                << "[UVDoc] Output is not Tensor."
                << std::endl;

        return cv::Mat();
    }

    //--------------------------------------------------------
    // Output Shape
    //--------------------------------------------------------
    auto tensorInfo =
            outputTensor
                    .GetTensorTypeAndShapeInfo();

    std::vector<int64_t> outputShape =
            tensorInfo.GetShape();

    if (outputShape.size() != 4)
    {
        std::cerr
                << "[UVDoc] Invalid Output Shape."
                << std::endl;

        return cv::Mat();
    }

    //--------------------------------------------------------
    // UVDoc_grid.onnx
    //
    // Output:
    // (1,H,W,2)
    //--------------------------------------------------------
    const int gridH =
            static_cast<int>(outputShape[1]);

    const int gridW =
            static_cast<int>(outputShape[2]);

    const int channel =
            static_cast<int>(outputShape[3]);

    if (channel != 2)
    {
        std::cerr
                << "[UVDoc] Grid Channel Error."
                << std::endl;

        return cv::Mat();
    }

    //--------------------------------------------------------
    // Grid Pointer
    //--------------------------------------------------------
    float* gridData =
            outputTensor.GetTensorMutableData<float>();

    //--------------------------------------------------------
    // Debug
    //--------------------------------------------------------
    std::cout
            << "Grid Shape : "
            << outputShape[0] << " "
            << outputShape[1] << " "
            << outputShape[2] << " "
            << outputShape[3]
            << std::endl;

    //--------------------------------------------------------
    // Postprocess
    //--------------------------------------------------------
    return Postprocess(
            gridData,
            gridH,
            gridW,
            originalRGB,
            originalWidth,
            originalHeight);
}
//------------------------------------------------------------
// Postprocess
//------------------------------------------------------------
cv::Mat UVDoc::Postprocess(
        const float* gridData,
        int gridH,
        int gridW,
        const cv::Mat& originalRGB,
        int originalWidth,
        int originalHeight)
{
    //--------------------------------------------------------
    // Grid(H,W,2)
    //--------------------------------------------------------
    cv::Mat grid(
            gridH,
            gridW,
            CV_32FC2,
            const_cast<float*>(gridData));

    //--------------------------------------------------------
    // Resize Grid
    //--------------------------------------------------------
    cv::Mat resizedGrid;

    cv::resize(
            grid,
            resizedGrid,
            cv::Size(
                    originalWidth,
                    originalHeight),
            0,
            0,
            cv::INTER_LINEAR);

    //--------------------------------------------------------
    // Split
    //--------------------------------------------------------
    std::vector<cv::Mat> channels;

    cv::split(
            resizedGrid,
            channels);

    //--------------------------------------------------------
    // map_x
    //--------------------------------------------------------
    cv::Mat mapX =
            (channels[0] + 1.0f) *
            (0.5f * (originalWidth - 1));

    //--------------------------------------------------------
    // map_y
    //--------------------------------------------------------
    cv::Mat mapY =
            (channels[1] + 1.0f) *
            (0.5f * (originalHeight - 1));

    mapX.convertTo(
            mapX,
            CV_32FC1);

    mapY.convertTo(
            mapY,
            CV_32FC1);

    //--------------------------------------------------------
    // Remap
    //--------------------------------------------------------
    cv::Mat rectifiedRGB;

    cv::remap(
            originalRGB,
            rectifiedRGB,
            mapX,
            mapY,
            cv::INTER_LINEAR,
            cv::BORDER_REPLICATE);

    //--------------------------------------------------------
    // RGB -> BGR
    //--------------------------------------------------------
    cv::Mat rectifiedBGR;

    cv::cvtColor(
            rectifiedRGB,
            rectifiedBGR,
            cv::COLOR_RGB2BGR);

    return rectifiedBGR;
}
