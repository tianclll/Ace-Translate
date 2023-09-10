
from DocImgTranslation.FastSAM.fastsam import FastSAM, FastSAMPrompt
import ast
import torch
from PIL import Image
import configparser
config = configparser.ConfigParser()
config.read("config.conf")
device = config.get("devices", "device")
def main(img_path,device=device,text_prompt="A sheet of white A4 paper",model_path='models/FastSAM.pt'):
    # load model
    model = FastSAM(model_path)
    input = Image.open(img_path)

    # 检查图像的旋转方向标志
    if hasattr(input, '_getexif'):  # 检查是否有EXIF数据
        exif = input._getexif()
        if exif is not None:
            orientation = exif.get(0x0112)  # 获取方向标志（0x0112是EXIF中的方向标志）
            if orientation is not None:
                if orientation == 1:  # 没有旋转
                    pass
                elif orientation == 3:  # 旋转180度
                    input = input.rotate(180, expand=True)
                elif orientation == 6:  # 顺时针旋转90度
                    input = input.rotate(270, expand=True)
                elif orientation == 8:  # 逆时针旋转90度
                    input = input.rotate(90, expand=True)
    input = input.convert("RGB")
    everything_results = model(
        input,
        device=device,
        retina_masks=True,
        imgsz=512,
        conf=0.7,
        iou=0.6
        )
    prompt_process = FastSAMPrompt(input, everything_results, device=device)
    # ann = prompt_process.text_prompt(text=text_prompt)
    # everything prompt
    ann = prompt_process.everything_prompt()
    return ann

if __name__ == "__main__":
    img_path="/Users/liuhongdi/计算机设计大赛/OCR图片文字识别/数据/jpg_pdf/1/16.jpg"
    main(img_path,device='cpu')
