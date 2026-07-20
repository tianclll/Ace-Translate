#pragma once

#include <vector>
#include <cstdint>

/**
 * @brief Fbank 特征提取
 * 输入：16kHz PCM 音频 → 输出：560-dim fbank 特征（80-dim × 7 帧拼接）
 */
class Fbank {
public:
    Fbank();
    ~Fbank() = default;

    /**
     * @brief 提取 fbank 特征
     * @param pcm 16kHz 16-bit mono PCM 数据
     * @param pcm_len 采样点数
     * @return fbank 特征 [frames, 560] 展平为一维
     */
    std::vector<float> extract(const short* pcm, int pcm_len);

    /** @brief 获取帧数 */
    int frameCount() const { return num_frames_; }

private:
    // 配置参数
    static constexpr int sample_rate_ = 16000;
    static constexpr float pre_emphasis_ = 0.97f;
    static constexpr float window_sec_ = 0.025f;    // 25ms
    static constexpr float stride_sec_ = 0.010f;    // 10ms
    static constexpr int num_fbank_ = 80;            // 80 Mel filters
    static constexpr int context_frames_ = 7;        // 左右各3帧拼接
    static constexpr int fft_size_ = 512;
    static constexpr int feature_dim_ = 560;         // 80 * 7

    int window_size_;   // 400 samples
    int stride_size_;   // 160 samples
    int num_frames_;    // 总帧数

    std::vector<double> hamming_window_;
    std::vector<std::vector<double>> mel_filter_bank_; // [num_fbank][fft_size_/2+1]

    void initHamming();
    void initMelFilterBank();
    void preEmphasis(const short* pcm, int len, std::vector<double>& out);
    std::vector<double> fftPower(const std::vector<double>& frame);
};

// 辅助：拼接上下文帧，输出固定 560 维
std::vector<float> applyContextFrames(const std::vector<std::vector<float>>& fbank_feats);
