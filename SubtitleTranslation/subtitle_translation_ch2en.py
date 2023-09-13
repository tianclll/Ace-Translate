from datetime import timedelta
import math
import cv2
import os
from paddleocr import PaddleOCR
import transformers
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
from moviepy.editor import VideoFileClip
from moviepy.editor import TextClip
from moviepy.editor import CompositeVideoClip
import glob
running = True
def cvsecs(time):
    print(time)
    time_parts = time.split(':')
    time_parts[-1] = time_parts[-1].replace(',','.')
    seconds = time_parts[-1].replace(',','.')
    seconds = seconds.split('.')[0]
    milliseconds = "0"
    if len(time_parts[-1].split('.')) > 1:
        milliseconds = time_parts[-1].split('.')[1]
    milliseconds = milliseconds.replace(',', '.')
    hours = time_parts[0]
    minutes = time_parts[1]

    return float(hours) * 3600 + float(minutes) * 60 + float(seconds) + float(milliseconds) / 1000

def gen_video(video_path, output_file):
    # 读取原始视频
    video = VideoFileClip(video_path)

    # 读取字幕文件
    subtitles = []
    with open("SubtitleTranslation/tmp.srt", "r") as srt_file:
        for line in srt_file:
            if not running:
                break
            line = line.strip()
            if line.isdigit():
                continue
            elif "-->" in line:
                start, end = line.split(" --> ")
            elif line:
                subtitles.append((start, end, line))
    print(subtitles)

    # 创建带有字幕的视频剪辑
    subtitled_video = video
    for subtitle in subtitles:
        if not running:
            break
        start_time = subtitle[0]
        end_time = subtitle[1]
        text = subtitle[2]

        # 将时间字符串转换为浮点数
        start_time = cvsecs(start_time)
        end_time = cvsecs(end_time)

        subtitle_clip = TextClip(text, fontsize=30, color='white', font='Arial', bg_color='black').set_position(
            ('center', 'bottom')).set_start(start_time).set_end(end_time)

        # 叠加字幕剪辑到视频上
        subtitled_video = CompositeVideoClip([subtitled_video.set_audio(None), subtitle_clip])

    # 设置视频的持续时间
    subtitled_video = subtitled_video.set_duration(video.duration)

    # 添加原始视频的音频
    subtitled_video = subtitled_video.set_audio(video.audio)

    # 生成输出视频文件
    subtitled_video.write_videofile(output_file, codec='libx264',audio_codec="aac")

def translate(video_path, output_path, type):
    src_video = cv2.VideoCapture(video_path)
    ocr = PaddleOCR(use_angle_cls=False, lang='ch',cls_model_dir='models/ocr/cls/ch_ppocr_mobile_v2.0_cls_infer',det_model_dir='models/ocr/det/ch/ch_PP-OCRv4_det_infer',rec_model_dir='models/ocr/rec/ch/ch_PP-OCRv4_rec_infer')
    model_path = 'models/translate/zh-en'
    tokenizer = AutoTokenizer.from_pretrained(model_path)
    model = AutoModelForSeq2SeqLM.from_pretrained(model_path)
    pipeline = transformers.pipeline("translation", model=model, tokenizer=tokenizer)

    # 获取视频帧率
    fps = round(src_video.get(cv2.CAP_PROP_FPS))
    # 计算视频总帧数
    total_frame = int(src_video.get(cv2.CAP_PROP_FRAME_COUNT))

    # 设置字幕序号
    num = 0
    save_text = []
    save_time = []
    last_res = 'None'
    print(total_frame)

    if type == "输出字幕文件":
        video_name = os.path.basename(video_path)
        video_name = video_name.replace('mp4','srt')
        srt_path = os.path.join(output_path, video_name)
    else:
        srt_path = 'SubtitleTranslation/tmp.srt'

    with open(srt_path, 'w+') as f:
        for i in range(total_frame):
            if not running:
                break
            success, frame = src_video.read()
            # 在视频字幕提取任务中，为了提速改为了每秒抽一帧，因此所有时间都是整数
            if i % math.floor(fps) == 0:
                if success:
                    result = ocr.ocr(frame[-120:-30, :], cls=True)
                    if len(result[0]) > 0:
                        res = result[0][0][1][0]
                        res = pipeline(res)[0]['translation_text']
                        if ((res[:-1] not in last_res) and (last_res[:-1] not in res)):
                            # 检测到新字幕，录入并开始计时
                            last_res = res
                            # 更新序号
                            num += 1
                            # 更新开始时间
                            start_time = "%s,%03d" % (timedelta(seconds=i // fps), 0 * 1000)
                            # 一段语句的结束时间，和语速也有一定关系，读者可以自行调整
                            end_time = "%s,%03d" % (timedelta(seconds=i // fps), 1.5 * 1000)
                            f.write(str(num) + '\n')
                            f.write(start_time + ' --> ' + end_time + '\n')
                            f.write(res + '\n')
                            f.write('\n')
                            # 保存文稿信息，用于后续标点恢复和问答任务
                            save_text.append(res)
                            # 保存字幕 + 时间信息，用于快速截取视频片段
                            save_time.append(res + ' ' + str(i // fps))

    if type == "输出视频":
        output_path = os.path.join(output_path, os.path.basename(video_path))
        gen_video(video_path, output_path)

    text = "已处理完成!"
    return text
def stop_translate():
    global running
    running = False
if __name__ == '__main__':
    translate('/Users/liuhongdi/Downloads/Trim.mp4', "/Users/liuhongdi/Downloads/", "输出视频")
