import configparser
import tkinter as tk

import pyperclip
import transformers
from pynput import keyboard
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
import utils
config = configparser.ConfigParser()
config.read("config.conf")
class TranslatorApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("全能翻译")
        self.geometry("300x100")

        self.init_ui()
        self.withdraw()  # 初始时隐藏窗口

    def init_ui(self):
        self.protocol("WM_DELETE_WINDOW", self.on_closing)  # 拦截窗口关闭事件

        # self.label = tk.Label(self, text="使用快捷键 Ctrl+Shift+F 进行截图并识别文字：")
        # self.label.pack(pady=10)

        self.frame = tk.Frame(self)
        self.frame.pack()

        self.text_frame = tk.Frame(self.frame)
        self.text_frame.pack(padx=10, pady=10)

        self.text_label = tk.Label(self.text_frame, text="翻译结果:")
        self.text_label.pack()

        self.text_box = tk.Text(self.text_frame, height=5, width=40)
        self.text_box.pack(fill=tk.BOTH, expand=True)
        translation_shortcut = config.get("shortcuts", "translation")
        self.listener = keyboard.GlobalHotKeys({
            translation_shortcut: self.text_ocr
        })
        self.listener.start()
    #
    # def get_selected_text(self):
    #     try:
    #         selected_text = subprocess.check_output(['pbpaste'], universal_newlines=True)
    #         return selected_text
    #     except subprocess.CalledProcessError:
    #         return ""
    def text_ocr(self):
        # keyboard1 = Controller()
        # 模拟按下 Ctrl + C 组合键
        # with keyboard1.pressed(Key.cmd):
        #     keyboard1.press('c')
        #     keyboard1.release('c')
        # time.sleep(0.5)
        # text = self.get_selected_text()
        text = pyperclip.paste()
        model_path = 'models/en-zh'
        tokenizer = AutoTokenizer.from_pretrained(model_path)
        translate_model = AutoModelForSeq2SeqLM.from_pretrained(model_path)
        pipeline = transformers.pipeline("translation", model=translate_model, tokenizer=tokenizer)
        translate_text = pipeline(text)
        translate_text = utils.do_sentence(translate_text[0]['translation_text'])
        self.display_translation(translate_text)
        print(translate_text)
        self.deiconify()  # 在截图完成后显示窗口
    def display_translation(self, text):
        self.text_box.delete("1.0", tk.END)  # 清空文本框
        self.text_box.insert(tk.END, text)
    def on_closing(self):
        self.withdraw()  # 仅隐藏窗口，不退出程序
if __name__ == "__main__":
    app = TranslatorApp()
    app.mainloop()
