import os
import utils
import subprocess
import tkinter as tk
import configparser
from PIL import Image, ImageTk
from pynput import keyboard
from paddleocr import PaddleOCR
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
import transformers

config = configparser.ConfigParser()
config.read("config.conf")
class ScreenShotTool(tk.Tk):
    def __init__(self):
        super().__init__()

        self.title("全能翻译")
        self.geometry("700x200")

        self.init_ui()
        self.withdraw()  # 初始时隐藏窗口

    def init_ui(self):
        self.protocol("WM_DELETE_WINDOW", self.on_closing)  # 拦截窗口关闭事件

        self.frame = tk.Frame(self)
        self.frame.pack()

        self.image_frame = tk.Frame(self.frame)
        self.image_frame.pack(side=tk.LEFT, padx=10, pady=10)

        self.text_frame = tk.Frame(self.frame)
        self.text_frame.pack(side=tk.RIGHT, padx=10, pady=10)
        self.image_label1 = tk.Label(self.image_frame,text="截图:")
        self.image_label1.pack()
        self.image_label = tk.Label(self.image_frame)
        self.image_label.pack()

        self.text_label = tk.Label(self.text_frame, text="翻译结果:")
        self.text_label.pack()

        self.text_box = tk.Text(self.text_frame, height=10, width=40)
        self.text_box.pack(fill=tk.BOTH, expand=True)
        screenshot_shortcut = config.get("shortcuts", "screenshot")
        self.listener = keyboard.GlobalHotKeys({
            screenshot_shortcut: self.capture_and_ocr
        })
        self.listener.start()

    def capture_and_ocr(self):
        ocr = PaddleOCR(use_angle_cls=True, lang='ch')
        model_path = 'models/en-zh'
        tokenizer = AutoTokenizer.from_pretrained(model_path)
        model = AutoModelForSeq2SeqLM.from_pretrained(model_path)
        pipeline = transformers.pipeline("translation", model=model, tokenizer=tokenizer)
        screenshot_path = "temp_screenshot.png"
        subprocess.run(["screencapture", "-i", screenshot_path])
        image = Image.open(screenshot_path)
        image.thumbnail((300, 300))
        photo = ImageTk.PhotoImage(image)
        self.image_label.config(image=photo)
        self.image_label.image = photo
        # 在这里添加图片OCR的代码，将识别出的文字显示在文本框中
        recognized_text = []
        results = ocr.ocr(screenshot_path, cls=True)
        for idx in range(len(results)):
            res = results[idx]
            for line in res:
                translate_text = pipeline(line[1][0])[0]['translation_text']
                translate_text = utils.do_sentence(translate_text)
                recognized_text.append(translate_text)
        translate_text = "\n".join(recognized_text)
        print(translate_text)
        self.text_box.delete("1.0", tk.END)  # 清空文本框
        self.text_box.insert(tk.END, translate_text)

        os.remove(screenshot_path)  # 删除临时截图文件

        self.deiconify()  # 在截图完成后显示窗口

    def on_closing(self):
        self.withdraw()  # 仅隐藏窗口，不退出程序

if __name__ == "__main__":
    app = ScreenShotTool()
    app.mainloop()
