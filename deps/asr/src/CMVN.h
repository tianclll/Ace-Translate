#pragma once

#include <vector>
#include <string>

/**
 * @brief CMVN (Cepstral Mean Variance Normalization)
 * 解析 Kaldi 格式的 am.mvn 文件并对 fbank 特征做归一化
 */
class CMVN {
public:
    CMVN() = default;
    ~CMVN() = default;

    /** @brief 加载 am.mvn 文件（Kaldi 格式） */
    bool load(const std::string& path);

    /** @brief 应用 CMVN 归一化（原地修改）
     *  @param feats [frames * dim] 展平特征
     *  @param num_frames 帧数
     *  @param dim 每帧维度（SenseVoice: 560）
     */
    void apply(std::vector<float>& feats, int num_frames, int dim) const;

    bool isLoaded() const { return !means_.empty(); }

private:
    std::vector<float> means_;   // AddShift（负均值）
    std::vector<float> vars_;    // Rescale（1/std）
};
