#include "CMVN.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>

bool CMVN::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "CMVN: cannot open " << path << std::endl;
        return false;
    }

    // 读取整个文件到字符串
    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // 查找 <AddShift> 标签后的数值块
    // 格式: <AddShift> 560 560 [v1 v2 ... v560]
    auto findBlock = [&](const std::string& tag, std::vector<float>& out) -> bool {
        size_t pos = content.find("<" + tag + ">");
        if (pos == std::string::npos) return false;

        // 跳过 "560 560 [" 或类似
        pos = content.find('[', pos);
        if (pos == std::string::npos) return false;
        pos++; // skip '['

        size_t end = content.find(']', pos);
        if (end == std::string::npos) return false;

        std::string numStr = content.substr(pos, end - pos);
        std::stringstream nums(numStr);
        float val;
        out.clear();
        while (nums >> val) {
            out.push_back(val);
        }
        return !out.empty();
    };

    if (!findBlock("AddShift", means_)) {
        std::cerr << "CMVN: <AddShift> not found in " << path << std::endl;
        return false;
    }

    if (!findBlock("Rescale", vars_)) {
        std::cerr << "CMVN: <Rescale> not found in " << path << std::endl;
        return false;
    }

    if (means_.size() != vars_.size()) {
        std::cerr << "CMVN: size mismatch means=" << means_.size()
                  << " vars=" << vars_.size() << std::endl;
        return false;
    }

    std::cout << "CMVN loaded: dim=" << means_.size() << std::endl;
    return true;
}

void CMVN::apply(std::vector<float>& feats, int num_frames, int dim) const {
    if (feats.empty() || num_frames <= 0 || dim <= 0) return;
    if ((int)means_.size() != dim) {
        std::cerr << "CMVN: dim mismatch, expected " << dim << " got " << means_.size() << std::endl;
        return;
    }

    for (int f = 0; f < num_frames; ++f) {
        float* frame = feats.data() + f * dim;
        for (int i = 0; i < dim; ++i) {
            frame[i] = (frame[i] + means_[i]) * vars_[i];
        }
    }
}
