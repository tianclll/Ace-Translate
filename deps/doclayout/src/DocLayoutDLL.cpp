#include "DocLayoutDLL.hpp"
#include "PPDocLayoutV2.hpp"

#include <opencv2/opencv.hpp>

#include <cstring>

extern "C"
{

void* CreatePPDocLayout()
{
    return new PPDocLayoutV2();
}

bool InitPPDocLayout(
        void* handle,
        const char* model_path,
        bool use_gpu,
        int device_id)
{
    if (!handle)
    {
        return false;
    }

    auto* detector =
            static_cast<PPDocLayoutV2*>(handle);

    return detector->Init(model_path, use_gpu, device_id);
}

int DetectLayout(void* handle,const unsigned char* data, int width, int height, int channels,LayoutResult* results,int max_count)
{
    if (!handle)
    {
        return 0;
    }

    cv::Mat img(height, width, CV_8UC3, const_cast<unsigned char*>(data));

    if (img.empty())
    {
        return 0;
    }

    auto* detector =
            static_cast<PPDocLayoutV2*>(handle);

    auto boxes =
            detector->Detect(img);

    int count =
            std::min(
                    (int)boxes.size(),
                    max_count
            );

    for (int i = 0; i < count; i++)
    {
        auto& src = boxes[i];
        auto& dst = results[i];

        dst.class_id =
                src.class_id;

        dst.score =
                src.score;

        dst.x =
                src.box.x;

        dst.y =
                src.box.y;

        dst.width =
                src.box.width;

        dst.height =
                src.box.height;

        memset(
                dst.label,
                0,
                sizeof(dst.label)
        );

        strncpy(
                dst.label,
                src.label.c_str(),
                sizeof(dst.label) - 1
        );
    }

    return count;
}

void DestroyPPDocLayout(
        void* handle)
{
    if (handle)
    {
        delete
                static_cast<PPDocLayoutV2*>(handle);
    }
}

}