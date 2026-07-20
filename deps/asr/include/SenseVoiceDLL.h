#pragma once

#ifdef ASR_EXPORTS
#define ASR_API __declspec(dllexport)
#else
#define ASR_API __declspec(dllimport)
#endif

extern "C" {

/**
 * @brief 创建 ASR 引擎实例
 * @param model_path model_quant.onnx 路径
 * @param tokens_path tokens.json 路径
 * @param mvn_path am.mvn（CMVN 文件）路径，可为空
 * @param use_gpu 是否启用 GPU
 * @return 引擎句柄，失败返回 nullptr
 */
ASR_API void* asr_create(const char* model_path, const char* tokens_path, const char* mvn_path, int use_gpu);

/**
 * @brief 识别音频
 * @param handle asr_create 返回的句柄
 * @param pcm 16kHz 16-bit mono PCM 数据
 * @param pcm_len PCM 采样点数
 * @return 识别结果字符串（UTF-8），调用者需 free
 */
ASR_API char* asr_recognize(void* handle, const short* pcm, int pcm_len);

/**
 * @brief 销毁引擎实例
 * @param handle asr_create 返回的句柄
 */
ASR_API void asr_destroy(void* handle);

}
