#define _USE_MATH_DEFINES
#include <cmath>
#include "Fbank.h"
#include <algorithm>
#include <cstring>
#include <opencv2/opencv.hpp>

Fbank::Fbank()
    : window_size_(static_cast<int>(window_sec_ * sample_rate_))
    , stride_size_(static_cast<int>(stride_sec_ * sample_rate_))
    , num_frames_(0)
{
    initHamming();
    initMelFilterBank();
}

void Fbank::initHamming() {
    hamming_window_.resize(window_size_);
    for (int i = 0; i < window_size_; ++i) {
        hamming_window_[i] = 0.54 - 0.46 * cos(2.0 * M_PI * i / (window_size_ - 1));
    }
}

void Fbank::initMelFilterBank() {
    int num_fft_bins = fft_size_ / 2 + 1;
    double low_freq_mel = 0.0;
    double high_freq_mel = 2595.0 * log10(1.0 + (sample_rate_ / 2.0) / 700.0);
    double mel_step = (high_freq_mel - low_freq_mel) / (num_fbank_ + 1);

    mel_filter_bank_.resize(num_fbank_, std::vector<double>(num_fft_bins, 0.0));

    for (int m = 0; m < num_fbank_; ++m) {
        double center_mel = low_freq_mel + (m + 1) * mel_step;
        double center_freq = 700.0 * (pow(10.0, center_mel / 2595.0) - 1.0);
        int center_bin = static_cast<int>(center_freq * fft_size_ / sample_rate_);

        // 左三角
        int left_bin = (m == 0) ? 0 :
            static_cast<int>(700.0 * (pow(10.0, (low_freq_mel + m * mel_step) / 2595.0) - 1.0) * fft_size_ / sample_rate_);
        for (int k = left_bin; k < center_bin; ++k) {
            mel_filter_bank_[m][k] = static_cast<double>(k - left_bin) / (center_bin - left_bin);
        }

        // 右三角
        int right_bin = static_cast<int>(700.0 * (pow(10.0, (low_freq_mel + (m + 2) * mel_step) / 2595.0) - 1.0) * fft_size_ / sample_rate_);
        if (right_bin >= num_fft_bins) right_bin = num_fft_bins - 1;
        for (int k = center_bin; k <= right_bin; ++k) {
            mel_filter_bank_[m][k] = static_cast<double>(right_bin - k) / (right_bin - center_bin);
        }
    }
}

void Fbank::preEmphasis(const short* pcm, int len, std::vector<double>& out) {
    out.resize(len);
    out[0] = static_cast<double>(pcm[0]) / 32768.0;
    for (int i = 1; i < len; ++i) {
        out[i] = static_cast<double>(pcm[i]) / 32768.0 - pre_emphasis_ * static_cast<double>(pcm[i - 1]) / 32768.0;
    }
}

std::vector<double> Fbank::fftPower(const std::vector<double>& frame) {
    int num_fft_bins = fft_size_ / 2 + 1;

    // 用 OpenCV 的 FFT（CV_64F）
    cv::Mat cv_frame(1, fft_size_, CV_64F, cv::Scalar(0));
    for (int i = 0; i < window_size_ && i < (int)frame.size(); ++i) {
        cv_frame.at<double>(0, i) = frame[i] * hamming_window_[i];
    }

    cv::Mat planes[] = {cv_frame, cv::Mat::zeros(1, fft_size_, CV_64F)};
    cv::Mat complex;
    cv::merge(planes, 2, complex);
    cv::dft(complex, complex);

    cv::Mat split_planes[2];
    cv::split(complex, split_planes);

    std::vector<double> power(num_fft_bins, 0.0);
    for (int k = 0; k < num_fft_bins; ++k) {
        double re = split_planes[0].at<double>(0, k);
        double im = split_planes[1].at<double>(0, k);
        power[k] = re * re + im * im;
    }

    return power;
}

std::vector<float> Fbank::extract(const short* pcm, int pcm_len) {
    if (!pcm || pcm_len < window_size_) {
        num_frames_ = 0;
        return {};
    }

    // 预加重
    std::vector<double> signal;
    preEmphasis(pcm, pcm_len, signal);

    // 分帧 + fbank 提取
    num_frames_ = (pcm_len - window_size_) / stride_size_ + 1;
    std::vector<std::vector<float>> all_fbanks;
    all_fbanks.reserve(num_frames_);

    for (int f = 0; f < num_frames_; ++f) {
        int start = f * stride_size_;
        std::vector<double> frame(signal.begin() + start, signal.begin() + start + std::min(window_size_, pcm_len - start));
        if ((int)frame.size() < window_size_) {
            frame.resize(window_size_, 0.0);
        }

        // FFT 功率谱
        auto power = fftPower(frame);
        int num_fft_bins = (int)power.size();

        // 功率谱加 floor 防止 log(0)
        for (int k = 0; k < num_fft_bins; ++k) {
            if (power[k] < 1e-10) power[k] = 1e-10;
        }

        // Mel 滤波 + log
        std::vector<float> fbank(num_fbank_, 0.0f);
        for (int m = 0; m < num_fbank_; ++m) {
            double sum = 0.0;
            for (int k = 0; k < num_fft_bins; ++k) {
                sum += power[k] * mel_filter_bank_[m][k];
            }
            fbank[m] = static_cast<float>(std::log(std::max(sum, 1e-10)));
            // 检查 NaN
            if (std::isnan(fbank[m])) {
                fbank[m] = -5.0f; // 兜底
            }
        }

        all_fbanks.push_back(std::move(fbank));
    }

    // 拼接上下文（7帧：左右各3帧）
    auto result = applyContextFrames(all_fbanks);
    // 更新帧数：applyContextFrames 会去掉前后各3帧 → 总帧数减6
    // 但若原帧数 <= 6 则 pad 为 1 帧
    int raw_frames = num_frames_;
    num_frames_ = (raw_frames > 6) ? (raw_frames - 6) : ((raw_frames > 0) ? 1 : 0);
    return result;
}

std::vector<float> applyContextFrames(const std::vector<std::vector<float>>& fbank_feats) {
    // 每帧 80-dim，拼接前后各3帧 → 560-dim
    static constexpr int ctx = 3;      // 左右各3帧
    static constexpr int dim = 80;     // fbank 维度
    static constexpr int ctx_dim = 560; // dim * (1 + ctx*2) = 80 * 7

    if (fbank_feats.empty()) return {};

    int num_frames = (int)fbank_feats.size();
    int num_valid_frames = num_frames - 2 * ctx;
    if (num_valid_frames <= 0) {
        // 帧数太少，pad 到能凑出至少1帧
        num_valid_frames = 1;
    }

    std::vector<float> result;
    result.reserve(num_valid_frames * ctx_dim);

    for (int i = 0; i < num_valid_frames; ++i) {
        int center = i + ctx;
        for (int j = -ctx; j <= ctx; ++j) {
            int idx = center + j;
            if (idx >= 0 && idx < num_frames) {
                const auto& src = fbank_feats[idx];
                result.insert(result.end(), src.begin(), src.end());
            } else {
                // padding 补零
                result.insert(result.end(), dim, 0.0f);
            }
        }
    }

    return result;
}
