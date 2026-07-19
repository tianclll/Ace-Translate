// include/db_postprocess.hpp
#ifndef DB_POSTPROCESS_HPP
#define DB_POSTPROCESS_HPP

#include <opencv2/opencv.hpp>
#include <vector>

namespace ocr {

    class DBPostProcess {
    public:
        DBPostProcess(
                float thresh = 0.2f,
                float box_thresh = 0.45f,
                float unclip_ratio = 1.4f,
                int max_candidates = 3000
        );

        std::vector<std::vector<cv::Point2f>> process(
                const cv::Mat& pred,
                int src_w,
                int src_h,
                float scale_x,
                float scale_y
        );

    private:
        float boxScoreFast(const cv::Mat& bitmap, const std::vector<cv::Point2f>& box);
        std::vector<cv::Point2f> unclip(const std::vector<cv::Point2f>& box, float ratio);
        std::pair<std::vector<cv::Point2f>, float> getMiniBoxes(const std::vector<cv::Point2f>& contour);

        float thresh_;
        float box_thresh_;
        float unclip_ratio_;
        int max_candidates_;
        int min_size_ = 3;
    };

} // namespace ocr

#endif // DB_POSTPROCESS_HPP