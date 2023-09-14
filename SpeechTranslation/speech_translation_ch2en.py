#! /usr/bin/env python
import os
import transformers
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
from paddlespeech.cli.asr.infer import ASRExecutor
from paddlespeech.cli.text.infer import TextExecutor
from pydub import AudioSegment
from pydub.utils import make_chunks
import configparser
config = configparser.ConfigParser()
config.read("config.conf")
device = config.get("devices", "device")
# 输入音频文件的路径（例如 MP3、FLAC、OGG 等）

def translate(input_audio_file):
    # 指定输出的 WAV 文件路径
    output_wav_file = "SpeechTranslation/output_audio.wav"
    # 确保输出文件夹存在

    audio_folder = "SpeechTranslation/output_chunks/"
    os.makedirs(audio_folder, exist_ok=True)
    # 使用 AudioSegment 加载音频文件
    audio = AudioSegment.from_file(input_audio_file)
    audio_length_seconds = len(audio) / 1000.0
    # 将音频文件保存为 WAV 格式
    audio.export(output_wav_file, format="wav")
    asr = ASRExecutor()
    text=''
    if audio_length_seconds > 50:
        # 切分保存的 WAV 文件为多个50秒的片段
        # 定义切分的时长（单位：毫秒）
        chunk_length_ms = 50000  # 50秒

        # 使用 AudioSegment 再次加载 WAV 文件
        wav_audio = AudioSegment.from_file(output_wav_file)

        # 切分音频文件成多个片段
        chunks = make_chunks(wav_audio, chunk_length_ms)


        # 保存切分后的音频片段
        for i, chunk in enumerate(chunks):
            chunk = chunk.set_frame_rate(16000)
            output_file = f"{audio_folder}{i + 1}.wav"
            chunk.export(output_file, format="wav")

        print("音频文件已成功转换为 WAV 格式，并切分为多个50秒的片段，保存在:", audio_folder)

        audio_files = [os.path.join(audio_folder, file) for file in os.listdir(audio_folder) if file.endswith(".wav")]
        # 对音频文件列表按文件名排序
        audio_files.sort()
        for audio in audio_files:
            result = asr(audio_file=audio,device=device)
            text = result+text
    else:
        text = asr(audio_file=output_wav_file, device=device)
    # text_punc = TextExecutor()
    # translate_text = text_punc(text=text)
    model_path = 'models/translate/zh-en'
    tokenizer = AutoTokenizer.from_pretrained(model_path)
    model = AutoModelForSeq2SeqLM.from_pretrained(model_path)
    pipeline = transformers.pipeline("translation", model=model, tokenizer=tokenizer)
    translate_text = pipeline(text)[0]['translation_text']
    # shutil.rmtree("exp")
    os.remove("SpeechTranslation/audio_recording.wav")
    os.remove("SpeechTranslation/output_audio.wav")
    return translate_text

