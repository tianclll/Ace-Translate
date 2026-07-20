#include "AudioCapture.h"
#include <cstring>

AudioCapture::AudioCapture() {
    for (int i = 0; i < num_bufs_; ++i) {
        memset(&waveHeaders_[i], 0, sizeof(WAVEHDR));
        buffer_[i].resize(buf_size_, 0);
    }
}

AudioCapture::~AudioCapture() {
    stop();
}

bool AudioCapture::start() {
    if (recording_) return true;

    all_data_.clear();
    all_data_.reserve(buf_size_ * 4); // 预分配12秒

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 16000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    MMRESULT res = waveInOpen(&hWaveIn_, WAVE_MAPPER, &wfx,
                               (DWORD_PTR)&waveInCallback, (DWORD_PTR)this,
                               CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) {
        hWaveIn_ = nullptr;
        return false;
    }

    // 准备 buffer
    for (int i = 0; i < num_bufs_; ++i) {
        waveHeaders_[i].lpData = (LPSTR)buffer_[i].data();
        waveHeaders_[i].dwBufferLength = buf_size_ * sizeof(short);
        waveHeaders_[i].dwFlags = 0;
        waveInPrepareHeader(hWaveIn_, &waveHeaders_[i], sizeof(WAVEHDR));
        waveInAddBuffer(hWaveIn_, &waveHeaders_[i], sizeof(WAVEHDR));
    }

    recording_ = true;
    waveInStart(hWaveIn_);
    return true;
}

std::vector<short> AudioCapture::stop() {
    if (!recording_ && hWaveIn_ == nullptr) return {};

    recording_ = false;

    if (hWaveIn_) {
        waveInStop(hWaveIn_);
        waveInReset(hWaveIn_);
        for (int i = 0; i < num_bufs_; ++i) {
            waveInUnprepareHeader(hWaveIn_, &waveHeaders_[i], sizeof(WAVEHDR));
        }
        waveInClose(hWaveIn_);
        hWaveIn_ = nullptr;
    }

    return std::move(all_data_);
}

void CALLBACK AudioCapture::waveInCallback(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                            DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    auto* self = reinterpret_cast<AudioCapture*>(dwInstance);
    if (!self || !self->recording_) return;

    if (uMsg == WIM_DATA) {
        auto* header = reinterpret_cast<WAVEHDR*>(dwParam1);
        if (!header) return;

        // 复制数据
        int samples = header->dwBytesRecorded / sizeof(short);
        if (samples > 0) {
            short* data = reinterpret_cast<short*>(header->lpData);
            size_t old_size = self->all_data_.size();
            self->all_data_.resize(old_size + samples);
            memcpy(self->all_data_.data() + old_size, data, samples * sizeof(short));
        }

        // 重新提交 buffer
        if (self->recording_) {
            header->dwBytesRecorded = 0;
            header->dwFlags = 0;
            waveInPrepareHeader(hwi, header, sizeof(WAVEHDR));
            waveInAddBuffer(hwi, header, sizeof(WAVEHDR));
        }
    }
}
