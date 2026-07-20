#pragma once

#include <Windows.h>
#include <vector>
#include <functional>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

/**
 * @brief Windows waveIn 录音封装
 * 16kHz 16-bit mono PCM
 */
class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    /** @brief 开始录音 */
    bool start();

    /** @brief 停止录音并获取数据 */
    std::vector<short> stop();

    /** @brief 是否正在录音 */
    bool isRecording() const { return recording_; }

    /** @brief 采样率 */
    static constexpr int sampleRate() { return 16000; }

private:
    static constexpr int buf_size_ = 16000 * 3; // 3秒缓冲区
    static constexpr int num_bufs_ = 2;

    HWAVEIN hWaveIn_ = nullptr;
    WAVEHDR waveHeaders_[num_bufs_];
    std::vector<short> buffer_[num_bufs_];
    std::vector<short> all_data_;
    bool recording_ = false;
    bool initialized_ = false;

    static void CALLBACK waveInCallback(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                         DWORD_PTR dwParam1, DWORD_PTR dwParam2);
};
