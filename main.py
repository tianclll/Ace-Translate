import glob
import os
import shutil
import wave
import pystray
import pyaudio
import threading
import subprocess
import tkinter as tk
from PIL import Image
from tkinter import ttk
from tkinter import filedialog, scrolledtext
from ttkthemes import ThemedStyle  # 导入 ThemedStyle


class TranslationApp:
    def __init__(self, root):
        self.root = root
        self.root.title("全能翻译")
        # 创建 ttk 样式dff
        self.style = ThemedStyle(root)
        self.style.set_theme("radiance")  # 设置 Radiance 主题
        self.style.configure("Selected.TLabel", background="#F6F4F3")  # 选中模块的标签样式
        self.style.configure("Unselected.TLabel", background="lightgray")  # 未选中模块的标签样式

        self.sidebar = ttk.Frame(root, width=200)
        self.sidebar.pack(side="left", fill="y",expand=True)

        self.modules = ["文本翻译","划词翻译", "截图翻译", "语音翻译", "视频翻译", "文件翻译", "文档图片翻译"]
        self.module_labels = []
        for module in self.modules:
            label_frame = tk.Frame(self.sidebar)
            label_frame.pack(fill="x")

            label_style = "Selected.TLabel" if module == self.modules[0] else "Unselected.TLabel"  # 设置不同模块的标签样式
            label = ttk.Label(label_frame, text=module, font=("Helvetica", 12), style=label_style)
            label.pack(fill="x", padx=10, pady=5)
            label.bind("<Button-1>", lambda event, m=module: self.run_module(m))
            self.module_labels.append(label)

        self.content = tk.Frame(root, padx=20, pady=20)
        self.content.pack(side="right", fill="both", expand=True)
        self.content.configure(bg="#F6F4F3")  # 设置背景颜色

        self.recording_thread = None  # 用于存储录音线程
        self.audio_file_path = None  # 存储录音文件的路径
        self.selected_module = None  # 保存当前选中的模块
        self.threads = {}  # 存储模块对应的线程
        self.translation_thread = None     # 创建一个用于翻译的线程
        self.voice_translation_thread = None  # 创建一个用于语音翻译的线程
        self.video_translation_thread = None
        self.run_module(self.modules[0])  # 默认显示第一个模块
        self.running_processes = {}  # 用于保存各模块的进程对象
        self.create_tray_icon()
        self.root.protocol('WM_DELETE_WINDOW', self.hide_window)
    def create_tray_icon(self):
        icon_path = "static/translate.png"  # 修改为你的图标路径

        # 读取图标并创建菜单项
        icon_image = Image.open(icon_path)
        menu = (
            pystray.MenuItem("打开", self.show_window),
            pystray.MenuItem("退出", self.quit_app)
        )

        # 创建系统托盘图标
        self.tray_icon = pystray.Icon("app_name", icon_image, "Ace Translate", menu)
        threading.Thread(target=self.tray_icon.run, daemon=True).start()
    def show_window(self):
        if not self.root.winfo_viewable():
            self.root.deiconify()  # 显示窗口
            self.root.lift()  # 将窗口置于顶层
        else:
            self.root.iconify()  # 隐藏窗口
    def hide_window(self):
        self.root.withdraw()
    def run_module(self, module):
        if module in self.threads and self.threads[module].is_alive():
            return  # 如果模块的线程正在运行，则不做任何操作
        self.selected_module = module
        self.update_sidebar_colors()
        self.clear_content()
        if module == "文本翻译":
            thread = threading.Thread(target=self.run_translation_module)
        elif module == "划词翻译":
            thread = threading.Thread(target=self.run_hyper_translation_module)
        elif module == "截图翻译":
            thread = threading.Thread(target=self.run_screenshot_module)
        elif module == "语音翻译":
            thread = threading.Thread(target=self.run_voice_translation_module)
        elif module == "视频翻译":
            thread = threading.Thread(target=self.run_video_module)
        elif module == "文件翻译":
            thread = threading.Thread(target=self.run_pdf_module)
        elif module == "文档图片翻译":
            thread = threading.Thread(target=self.run_image_module)

        self.threads[module] = thread
        thread.start()

    def clear_content(self):
        for widget in self.content.winfo_children():
            widget.destroy()

    def update_sidebar_colors(self):
        for label in self.module_labels:
            label_frame = label.master
            if label.cget("text") == self.selected_module:
                label.config(style="Selected.TLabel")  # 使用 Selected.TLabel 样式
                label_frame.config(bg="#F6F4F3")
            else:
                label.config(style="Unselected.TLabel")  # 使用 Unselected.TLabel 样式
                label_frame.config(bg="lightgray")

    def stop_module(self, module_name):
        if module_name == "视频翻译":
            from SubtitleTranslation import subtitle_translation_ch2en,subtitle_translation_en2ch
            subtitle_translation_ch2en.stop_translate()
            subtitle_translation_en2ch.stop_translate()
        if module_name in self.running_processes:
            process = self.running_processes[module_name]
            if os.name == 'nt':  # Windows系统
                process.terminate()
            else:  # Linux或类Unix系统
                process.kill()
            del self.running_processes[module_name]
    def run_translation_module(self):

        translation_module = ttk.Frame(self.content)
        label = ttk.Label(translation_module, text="文本翻译", font=("Helvetica", 16))
        label.pack()

        # 添加单选框，默认选中"汉译英"
        self.translation_direction = tk.StringVar(value="汉译英")
        direction_frame = ttk.Frame(translation_module)
        direction_frame.pack()

        to_english_radio = ttk.Radiobutton(direction_frame, text="汉译英", variable=self.translation_direction,
                                           value="汉译英")
        to_english_radio.pack(side="left")

        to_chinese_radio = ttk.Radiobutton(direction_frame, text="英译汉", variable=self.translation_direction,
                                           value="英译汉")
        to_chinese_radio.pack(side="left")

        # 创建文本输入框，将其添加到 translation_module
        entry = ttk.Entry(translation_module, width=20)
        entry.pack(padx=10, pady=10)

        # 创建翻译按钮，使用 lambda 包装函数
        translate_button = ttk.Button(translation_module, text="翻译",
                                      command=lambda: self.start_translation_module(entry.get()))
        translate_button.pack(pady=10)

        # 创建文本标签用于显示翻译结果，将其添加到 translation_module
        self.translate_translated_text = tk.StringVar()
        result_entry = ttk.Entry(translation_module, textvariable=self.translate_translated_text, width=20)
        result_entry.configure(state="readonly")
        result_entry.pack(padx=10, pady=10)

        # 添加 translation_module 到 content
        translation_module.pack(fill="both", expand=True)

    def start_translation_module(self, translate_text):
        if self.translation_thread is not None and self.translation_thread.is_alive():
            return

        self.translation_thread = threading.Thread(target=self.run_translation, args=(translate_text,))
        self.translation_thread.start()


    def run_translation(self,translate_text):
        from Translation import translation_ch2en, translation_en2ch
        translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
        if translation_direction == "英译汉":
            text = translation_en2ch.translate(translate_text)
        else:
            text = translation_ch2en.translate(translate_text)
        self.translate_translated_text.set(text)
    def run_hyper_translation_module(self):
        module_name = "划词翻译"

        translation_module = ttk.Frame(self.content)

        label = ttk.Label(translation_module, text="划词翻译",font=("Helvetica", 16))
        label.pack()
        # 添加单选框，默认选中"汉译英"
        self.translation_direction = tk.StringVar(value="汉译英")
        direction_frame = ttk.Frame(translation_module)
        direction_frame.pack()

        to_english_radio = ttk.Radiobutton(direction_frame, text="汉译英", variable=self.translation_direction, value="汉译英")
        to_english_radio.pack(side="left")

        to_chinese_radio = ttk.Radiobutton(direction_frame, text="英译汉", variable=self.translation_direction, value="英译汉")
        to_chinese_radio.pack(side="left")
        start_button = ttk.Button(translation_module, text="开始", command=lambda: self.start_hyper_translation_module(module_name))
        start_button.pack()

        stop_button = ttk.Button(translation_module, text="结束", command=lambda: self.stop_module(module_name))
        stop_button.pack()

        translation_module.pack(fill="both", expand=True)

    def start_hyper_translation_module(self, module_name):
        if module_name not in self.running_processes:
            translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
            if translation_direction == "英译汉":
                process = subprocess.Popen(['python', 'HyperTranslation/hyper_translation_en2ch.py',translation_direction])
            else:
                process = subprocess.Popen(['python', 'HyperTranslation/hyper_translation_ch2en.py', translation_direction])
            self.running_processes[module_name] = process

    # 其他模块函数省略...
    def run_screenshot_module(self):
        module_name = "截图翻译"
        screenshot_module = ttk.Frame(self.content)
        label = ttk.Label(screenshot_module, text="截图翻译", font=("Helvetica", 16))
        label.pack()
        # 添加单选框，默认选中"汉译英"
        self.translation_direction = tk.StringVar(value="汉译英")
        direction_frame = ttk.Frame(screenshot_module)
        direction_frame.pack()

        to_english_radio = ttk.Radiobutton(direction_frame, text="汉译英", variable=self.translation_direction, value="汉译英",)
        to_english_radio.pack(side="left")

        to_chinese_radio = ttk.Radiobutton(direction_frame, text="英译汉", variable=self.translation_direction, value="英译汉")
        to_chinese_radio.pack(side="left")
        start_button = ttk.Button(screenshot_module, text="开始", command=lambda: self.start_screenshot_module(module_name))
        start_button.pack()

        stop_button = ttk.Button(screenshot_module, text="结束", command=lambda: self.stop_module(module_name))
        stop_button.pack()

        screenshot_module.pack(fill="both", expand=True)
    def start_screenshot_module(self, module_name):
        if module_name not in self.running_processes:
            translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
            if translation_direction == "英译汉":
                process = subprocess.Popen(['python', 'ScreenshotTranslation/screenshot_translation_en2ch.py'])
            else:
                process = subprocess.Popen(['python', 'ScreenshotTranslation/screenshot_translation_ch2en.py'])
            self.running_processes[module_name] = process

    def run_voice_translation_module(self):
        self.voice_translation_module = ttk.Frame(self.content)
        # self.voice_translation_module.pack_forget()  # 隐藏模块

        label = ttk.Label(self.voice_translation_module, text="语音翻译", font=("Helvetica", 16))
        label.pack()

        self.translation_direction = tk.StringVar(value="英译汉")
        direction_frame = ttk.Frame(self.voice_translation_module)
        direction_frame.pack()

        to_english_radio = ttk.Radiobutton(direction_frame, text="英译汉", variable=self.translation_direction,
                                           value="英译汉")
        to_english_radio.pack(side="left")

        to_chinese_radio = ttk.Radiobutton(direction_frame, text="汉译英", variable=self.translation_direction,
                                           value="汉译英")
        to_chinese_radio.pack(side="left")

        # 使用ttk.LabelFrame来包裹按钮
        button_frame = ttk.LabelFrame(self.voice_translation_module, text="录音控制")
        button_frame.pack()

        # 启动录音按钮
        self.record_audio_button = ttk.Button(button_frame, text="开始录音", command=self.start_record)
        self.record_audio_button.grid(row=0, column=0, padx=10, pady=5)

        # 停止录音按钮
        self.stop_button = ttk.Button(button_frame, text="停止录音", command=self.stop_record, state=tk.DISABLED)
        self.stop_button.grid(row=0, column=1, padx=10, pady=5)

        # 上传音频文件按钮
        self.upload_audio_button = ttk.Button(button_frame, text="上传音频文件", command=self.upload_audio_file)
        self.upload_audio_button.grid(row=1, column=0, padx=10, pady=5)

        # 开始翻译按钮
        self.start_translation_button = ttk.Button(button_frame, text="开始翻译",
                                                   command=lambda: self.start_voice_translation("语音翻译"),
                                                   state=tk.DISABLED)
        self.start_translation_button.grid(row=1, column=1, padx=10, pady=5)

        # 翻译结果文本框
        self.voice_translation_result = scrolledtext.ScrolledText(self.voice_translation_module, padx=10, pady=10,
                                                                  height=6, width=40)
        self.voice_translation_result.pack()

        self.voice_translation_module.pack(fill="both", expand=True)

    def start_voice_translation(self,module_name):

        if self.voice_translation_thread is not None and self.voice_translation_thread.is_alive():
            return

        self.voice_translation_thread = threading.Thread(target=self.run_voice_translation, args=(module_name,))
        self.voice_translation_thread.start()

    is_recording = False  # 添加一个标志来控制录音状态
    def run_voice_translation(self, module_name):
        # 这个函数包含了你之前在 start_voice_translation_module 函数中的语音翻译逻辑
        from SpeechTranslation import speech_translation_ch2en, speech_translation_en2ch

        # 获取用户选择的翻译方向
        translation_direction = self.translation_direction.get()

        if translation_direction == "英译汉":
            text = speech_translation_en2ch.translate(self.audio_file_path)
        else:
            text = speech_translation_ch2en.translate(self.audio_file_path)

        # 在文本框中显示翻译文本
        self.voice_translation_result.delete("1.0", "end")  # 清空文本框
        self.voice_translation_result.insert("end", str(text))  # 插入翻译文本

    def start_record(self):
        # 启用停止录音按钮
        self.stop_button.config(state=tk.NORMAL)
        # 禁用开始录音按钮
        self.record_audio_button.config(state=tk.DISABLED)
        # 禁用上传音频文件按钮
        self.upload_audio_button.config(state=tk.DISABLED)

        self.audio_thread = threading.Thread(target=self.record_audio)
        self.audio_thread.start()

    def record_audio(self):
        CHUNK = 1024
        FORMAT = pyaudio.paInt16
        CHANNELS = 1
        RATE = 16000
        audio = pyaudio.PyAudio()

        stream = audio.open(format=FORMAT, channels=CHANNELS,
                            rate=RATE, input=True,
                            frames_per_buffer=CHUNK)
        frames = []

        print("开始录音...")
        global is_recording
        is_recording = True
        while is_recording:
            data = stream.read(CHUNK)
            frames.append(data)
        print("录音结束...")

        stream.stop_stream()
        stream.close()
        audio.terminate()

        # 保存录音文件
        self.audio_file_path = "SpeechTranslation/audio_recording.wav"
        with wave.open(self.audio_file_path, 'wb') as wf:
            wf.setnchannels(CHANNELS)
            wf.setsampwidth(audio.get_sample_size(FORMAT))
            wf.setframerate(RATE)
            wf.writeframes(b''.join(frames))

        # 启用开始录音按钮
        self.record_audio_button.config(state=tk.NORMAL)
        # 启用上传音频文件按钮
        self.upload_audio_button.config(state=tk.NORMAL)
        # 禁用停止录音按钮
        self.stop_button.config(state=tk.DISABLED)
        # 启用开始翻译按钮
        self.start_translation_button.config(state=tk.NORMAL)
    def upload_audio_file(self):
        file_path = filedialog.askopenfilename(filetypes=[("Audio Files", "*.wav *.mp3")])
        if file_path:
            self.audio_file_path = file_path
            self.start_translation_button.config(state=tk.NORMAL)
    def stop_record(self):
        global is_recording  # 使用全局标志来控制录音状态
        is_recording = False  # 停止录音
        # 禁用停止录音按钮
        self.stop_button.config(state=tk.DISABLED)
        # 启用开始录音按钮
        self.record_audio_button.config(state=tk.NORMAL)
        # 启用上传音频文件按钮
        self.upload_audio_button.config(state=tk.NORMAL)
        # 禁用开始翻译按钮
        self.start_translation_button.config(state=tk.DISABLED)
    def run_video_module(self):
        module_name = "视频翻译"
        video_module = ttk.Frame(self.content)
        label = ttk.Label(video_module, text="视频翻译", font=("Helvetica", 16))
        label.grid(row=0, column=0, columnspan=2)

        # 添加单选框，默认选中"汉译英"
        self.translation_direction = tk.StringVar(value="汉译英")
        direction_frame = ttk.Frame(video_module)
        direction_frame.grid(row=1, column=0, columnspan=2, pady=(10, 0))

        to_english_radio = ttk.Radiobutton(direction_frame, text="汉译英", variable=self.translation_direction,
                                           value="汉译英")
        to_english_radio.grid(row=0, column=0)

        to_chinese_radio = ttk.Radiobutton(direction_frame, text="英译汉", variable=self.translation_direction,
                                           value="英译汉")
        to_chinese_radio.grid(row=0, column=1)

        # upload_button = ttk.Button(video_module, text="上传文件", command=lambda: self.upload_pdf(module_name))
        # upload_button.grid(row=3, column=0, columnspan=2, pady=(10, 0))

        # 添加单选框，默认选中"汉译英"
        self.output_type = tk.StringVar(value="输出字幕文件")
        type = ttk.Frame(video_module)
        type.grid(row=2, column=0, columnspan=2, pady=(10, 0))

        to_srt = ttk.Radiobutton(type, text="输出字幕文件", variable=self.output_type,
                                           value="输出字幕文件")
        to_srt.grid(row=0, column=0)

        to_video = ttk.Radiobutton(type, text="输出视频", variable=self.output_type,
                                           value="输出视频")
        to_video.grid(row=0, column=1)
        upload_button = ttk.Button(video_module, text="上传视频文件", command=lambda: self.upload_video(module_name))
        upload_button.grid(row=3, column=0, columnspan=2, pady=(10, 0))


        self.save_path = tk.StringVar()
        save_path_entry = ttk.Entry(video_module, textvariable=self.save_path)
        save_path_entry.grid(row=4, column=0, pady=(10, 0))

        choose_path_button = ttk.Button(video_module, text="选择输出路径", command=self.choose_video_save_path)
        choose_path_button.grid(row=4, column=1, pady=(10, 0))

        start_button = ttk.Button(video_module, text="开始", command=lambda: self.start_video_module(module_name))
        start_button.grid(row=5, column=0, pady=(10, 0),sticky="w")

        stop_button = ttk.Button(video_module, text="结束", command=lambda: self.stop_module(module_name))
        stop_button.grid(row=5, column=1, pady=(10, 0),sticky="w")

        video_module.grid(row=0, column=0, sticky="nsew")

        self.video_translation_result = scrolledtext.ScrolledText(video_module, padx=10, pady=10, height=2, width=40)
        self.video_translation_result.grid(row=6, column=0, columnspan=2, pady=(10, 0))

        self.video_translation_result.grid(row=6, column=0, columnspan=2, pady=(10, 0))

        self.video_module = video_module
    def upload_video(self, module_name):
        file_path = filedialog.askopenfilename(filetypes=[("Video Files", "*.mp4")])
        if file_path:
            self.video_file_path = file_path
    def choose_video_save_path(self):
        chosen_path = filedialog.askdirectory()
        if chosen_path:
            self.save_path.set(chosen_path)
            # self.save_path = chosen_path
            # save_path_entry = ttk.Entry(self.video_module, textvariable=self.save_path)
            # save_path_entry.grid(row=3, column=0, pady=(10, 0))
    def start_video_module(self,module_name):
        if self.video_translation_thread is not None and self.video_translation_thread.is_alive():
            return
        self.video_translation_thread = threading.Thread(target=self.run_video_translation)
        self.video_translation_thread.start()

    def run_video_translation(self):
        # 这个函数包含了你之前在 start_voice_translation_module 函数中的语音翻译逻辑
        from SubtitleTranslation import subtitle_translation_ch2en, subtitle_translation_en2ch
        self.video_translation_result.delete("1.0", "end")  # 清空文本框
        self.video_translation_result.insert("end", "开始运行...\n")  # 插入翻译文本
        # 获取用户选择的翻译方向
        translation_direction = self.translation_direction.get()
        output_type = self.output_type.get()
        if translation_direction == "英译汉":
            text = subtitle_translation_en2ch.translate(self.video_file_path, self.save_path.get(), output_type)
        else:
            text = subtitle_translation_ch2en.translate(self.video_file_path, self.save_path.get(), output_type)

        # 在文本框中显示翻译文本
        # self.video_translation_result.delete("1.0", "end")  # 清空文本框
        self.video_translation_result.insert("end", str(text))  # 插入翻译文本
    def start_pdf_module(self,module_name):
        if module_name not in self.running_processes:
            log_message = f"开始运行 {module_name} 模块...\n将存入{self.save_path.get()}中."
            self.log_text.insert("end", log_message)
            self.log_text.see("end")  # 自动滚动到底部
            file_extension = self.pdf_file_path.split(".")[-1]
            if file_extension in ['xls','xlsx']:
                translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
                if translation_direction == "英译汉":
                    process = subprocess.Popen(
                        ['python', 'FileTranslation/Excel/excel_translation_en2ch.py', '{}'.format(self.pdf_file_path),
                         '{}'.format(self.save_path.get())])
                else:
                    process = subprocess.Popen(
                        ['python', 'FileTranslation/Excel/excel_translation_ch2en.py', '{}'.format(self.pdf_file_path),
                         '{}'.format(self.save_path.get())])
                self.running_processes[module_name] = process
            elif file_extension == 'pdf':
                translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
                if translation_direction == "英译汉":
                    process = subprocess.Popen(['python', 'FileTranslation/PDF/pdf_translation_en2ch.py','{}'.format(self.pdf_file_path),'{}'.format(self.save_path.get())])
                else:
                    process = subprocess.Popen(['python', 'FileTranslation/PDF/pdf_translation_ch2en.py', '{}'.format(self.pdf_file_path),'{}'.format(self.save_path.get())])
                self.running_processes[module_name] = process
            elif file_extension == 'txt':
                translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
                if translation_direction == "英译汉":
                    process = subprocess.Popen(['python', 'FileTranslation/TXT/txt_translation_en2ch.py','{}'.format(self.pdf_file_path),'{}'.format(self.save_path.get())])
                else:
                    process = subprocess.Popen(['python', 'FileTranslation/TXT/txt_translation_ch2en.py', '{}'.format(self.pdf_file_path),'{}'.format(self.save_path.get())])
                self.running_processes[module_name] = process
            elif file_extension == 'docx':
                translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
                if translation_direction == "英译汉":
                    process = subprocess.Popen(
                        ['python', 'FileTranslation/Word/word_translation_en2ch.py', '{}'.format(self.pdf_file_path),
                         '{}'.format(self.save_path.get())])
                else:
                    process = subprocess.Popen(
                        ['python', 'FileTranslation/Word/word_translation_ch2en.py', '{}'.format(self.pdf_file_path),
                         '{}'.format(self.save_path.get())])
                self.running_processes[module_name] = process
            elif file_extension == 'pptx':
                translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
                if translation_direction == "英译汉":
                    process = subprocess.Popen(
                        ['python', 'FileTranslation/PPT/ppt_translation_en2ch.py', '{}'.format(self.pdf_file_path),
                         '{}'.format(self.save_path.get())])
                else:
                    process = subprocess.Popen(
                        ['python', 'FileTranslation/PPT/ppt_translation_ch2en.py', '{}'.format(self.pdf_file_path),
                         '{}'.format(self.save_path.get())])
                self.running_processes[module_name] = process
            else:
                translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
                if translation_direction == "英译汉":
                    process = subprocess.Popen(
                        ['python', 'FileTranslation/Image/image_translation_en2ch.py', '{}'.format(self.pdf_file_path),
                         '{}'.format(self.save_path.get())])
                else:
                    process = subprocess.Popen(
                        ['python', 'FileTranslation/Image/image_translation_ch2en.py', '{}'.format(self.pdf_file_path),
                         '{}'.format(self.save_path.get())])
                self.running_processes[module_name] = process
    def run_pdf_module(self):
        module_name = "文件翻译"
        pdf_module = ttk.Frame(self.content)
        label = ttk.Label(pdf_module, text="文件翻译", font=("Helvetica", 16))
        label.grid(row=0, column=0, columnspan=2)

        # 添加单选框，默认选中"汉译英"
        self.translation_direction = tk.StringVar(value="汉译英")
        direction_frame = ttk.Frame(pdf_module)
        direction_frame.grid(row=1, column=0, columnspan=2, pady=(10, 0))

        to_english_radio = ttk.Radiobutton(direction_frame, text="汉译英", variable=self.translation_direction,
                                           value="汉译英")
        to_english_radio.grid(row=0, column=0)

        to_chinese_radio = ttk.Radiobutton(direction_frame, text="英译汉", variable=self.translation_direction,
                                           value="英译汉")
        to_chinese_radio.grid(row=0, column=1)

        upload_button = ttk.Button(pdf_module, text="上传文件", command=lambda: self.upload_pdf(module_name))
        upload_button.grid(row=2, column=0, columnspan=2, pady=(10, 0))


        self.save_path = tk.StringVar()
        save_path_entry = ttk.Entry(pdf_module, textvariable=self.save_path)
        save_path_entry.grid(row=3, column=0, pady=(10, 0))

        choose_path_button = ttk.Button(pdf_module, text="选择输出路径", command=self.choose_pdf_save_path)
        choose_path_button.grid(row=3, column=1, pady=(10, 0))

        start_button = ttk.Button(pdf_module, text="开始", command=lambda: self.start_pdf_module(module_name))
        start_button.grid(row=4, column=0, pady=(10, 0),sticky="w")

        stop_button = ttk.Button(pdf_module, text="结束", command=lambda: self.stop_module(module_name))
        stop_button.grid(row=4, column=1, pady=(10, 0),sticky="w")

        pdf_module.grid(row=0, column=0, sticky="nsew")

        self.log_text = scrolledtext.ScrolledText(pdf_module, padx=10, pady=10, height=2, width=40)
        self.log_text.grid(row=5, column=0, columnspan=2, pady=(10, 0))

        self.log_text.grid(row=5, column=0, columnspan=2, pady=(10, 0))

        self.pdf_module = pdf_module  # 存储 PDF 翻译模块的 Frame

    def choose_pdf_save_path(self):
        chosen_path = filedialog.askdirectory()
        if chosen_path:
            self.save_path.set(chosen_path)
            # self.save_path = chosen_path
            save_path_entry = ttk.Entry(self.pdf_module, textvariable=self.save_path)
            save_path_entry.grid(row=3, column=0, pady=(10, 0))
    def choose_img_save_path(self):
        chosen_path = filedialog.askdirectory()
        if chosen_path:
            self.save_path.set(chosen_path)
            # self.save_path = chosen_path
            save_path_entry = ttk.Entry(self.image_module, textvariable=self.save_path)
            save_path_entry.grid(row=3, column=0, pady=(10, 0))
    def upload_pdf(self, module_name):
        filetypes = [
            ("PDF Files", "*.pdf"),
            ("Image Files", ("*.png", "*.jpg", "*.jpeg")),
            ("Excel Files", ("*.xls", "*.xlsx")),
            ("Text Files", "*.txt"),
            ("Word Files","*.docx"),
            ("PowerPoint Files","*.pptx")
        ]
        file_path = filedialog.askopenfilename(filetypes=filetypes)
        if file_path:
            self.pdf_file_path = file_path
    def start_image_module(self,module_name):
        if module_name not in self.running_processes:
            log_message = f"开始运行 {module_name} 模块...\n将存入{self.save_path.get()}中."
            self.log_text.insert("end", log_message)
            self.log_text.see("end")  # 自动滚动到底部
            translation_direction = self.translation_direction.get()  # 获取选择的翻译方向
            if translation_direction == "英译汉":
                process = subprocess.Popen(['python','DocImgTranslation/ocr_image_en2ch.py','-i',f'{self.pdf_file_path}','-o',f'{self.save_path.get()}'])
            else:

                process = subprocess.Popen(['python','DocImgTranslation/ocr_image_ch2en.py','-i',f'{self.pdf_file_path}','-o',f'{self.save_path.get()}'])
            self.running_processes[module_name] = process


    def run_image_module(self):

        module_name = "文档图片翻译"
        image_module = ttk.Frame(self.content)
        label = ttk.Label(image_module, text="文档图片翻译", font=("Helvetica", 16))
        label.grid(row=0, column=0, columnspan=2)

        direction_frame = ttk.Frame(image_module)
        direction_frame.grid(row=1, column=0, columnspan=2, pady=(10, 0))

        to_english_radio = ttk.Radiobutton(direction_frame, text="汉译英", variable=self.translation_direction,
                                           value="汉译英")
        to_english_radio.pack(side="left")

        to_chinese_radio = ttk.Radiobutton(direction_frame, text="英译汉", variable=self.translation_direction,
                                           value="英译汉")
        to_chinese_radio.pack(side="left")

        upload_button = ttk.Button(image_module, text="上传图片文件", command=lambda: self.upload_image(module_name))
        upload_button.grid(row=2, column=0, columnspan=2, pady=(10, 0))

        self.save_path = tk.StringVar()
        save_path_entry = ttk.Entry(image_module, textvariable=self.save_path)
        save_path_entry.grid(row=3, column=0, pady=(10, 0), sticky="ew")

        choose_path_button = ttk.Button(image_module, text="选择输出路径", command=self.choose_img_save_path)
        choose_path_button.grid(row=3, column=1, pady=(10, 0), sticky="w")

        start_button = ttk.Button(image_module, text="开始", command=lambda: self.start_image_module(module_name))
        start_button.grid(row=4, column=0, pady=(10, 0), sticky="w")

        stop_button = ttk.Button(image_module, text="结束", command=lambda: self.stop_module(module_name))
        stop_button.grid(row=4, column=1, pady=(10, 0), sticky="w")
        image_module.grid(row=0, column=0, sticky="nsew")
        self.log_text = scrolledtext.ScrolledText(image_module, padx=10, pady=10, height=2, width=40)
        self.log_text.grid(row=5, column=0, columnspan=2, pady=(10, 0))
        self.log_text.grid(row=5, column=0, columnspan=2, pady=(10, 0))
        image_module.pack(fill="both", expand=True)
        self.image_module = image_module
    def upload_image(self, module_name):
        file_path = filedialog.askopenfilename(filetypes=[("image Files", "*.jpg *.png")])
        if file_path:
            self.pdf_file_path = file_path
    def quit_app(self):
        if os.path.exists("exp"):
            shutil.rmtree("exp")
        mp3_pattern = "*.mp3"
        # 使用 glob 模块查找匹配的文件
        mp3_files = glob.glob(mp3_pattern)
        if mp3_files:
            for mp3_file in mp3_files:
                os.remove(mp3_file)
        # 终止所有模块线程
        for module, thread in self.threads.items():
            if thread.is_alive():
                thread.join()
        for i in self.running_processes.keys():
            process = self.running_processes[i]
            if os.name == 'nt':  # Windows系统
                process.terminate()
            else:  # Linux或类Unix系统
                process.kill()
        self.root.quit()
if __name__ == '__main__':

    root = tk.Tk()
    app = TranslationApp(root)
    root.mainloop()
