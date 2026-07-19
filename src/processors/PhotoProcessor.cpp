#include "docmind/processors/PhotoProcessor.hpp"
#include <stdexcept>

namespace docmind {
    std::string PhotoProcessor::process(const cv::Mat& /*image*/, const std::string& /*target_language*/) {
        throw std::runtime_error("PhotoProcessor not implemented yet.");
    }
}