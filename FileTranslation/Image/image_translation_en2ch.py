import os
import cv2
import numpy as np
import sys
sys.path.append(os.getcwd())
import my_utils
from bs4 import BeautifulSoup
from PIL import Image,ImageFont,ImageDraw
from paddleocr import PPStructure,save_structure_res,PaddleOCR
from paddleocr.ppstructure.recovery.recovery_to_doc import sorted_layout_boxes, convert_info_docx
import transformers
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
from paddleocr.ppocr.utils.logging import get_logger
import sys
logger = get_logger()
def image_translate(img,text_region,text):
    x_coords = [text_region[0][0], text_region[1][0], text_region[2][0], text_region[3][0]]
    y_coords = [text_region[0][1],text_region[1][1],text_region[2][1],text_region[3][1]]
    x1, x2 = int(min(x_coords)), int(max(x_coords))
    y1, y2 = int(min(y_coords)), int(max(y_coords))
    img[y1:y2, x1:x2] = (255, 255, 255)  # 将区域填充为背景色
    bbox_width = max(x_coords) - min(x_coords)
    bbox_height = max(y_coords) - min(y_coords)
    font_size = 12
    # 将NumPy数组转换为PIL Image对象
    image_pil = Image.fromarray(img)
    draw = ImageDraw.Draw(image_pil)
    font = ImageFont.truetype("static/simfang.ttf", font_size)
    while draw.textsize(text, font=font)[0] < bbox_width and draw.textsize(text, font=font)[
        1] < bbox_height:
        font_size += 1
        font = ImageFont.truetype("static/simfang.ttf", font_size)
    text_x = text_region[0][0]
    text_y = text_region[0][1]
    font_color = (0, 0, 0)
    draw.text((text_x, text_y), text, font=font, fill=font_color)
    # image_pil.save("output_image.jpg")
    # 将修改后的PIL Image对象转回OpenCV格式
    img = cv2.cvtColor(np.array(image_pil), cv2.COLOR_RGB2BGR)
    return img
def table_translate(html_table,translate_model):
    # 解析 HTML
    soup = BeautifulSoup(html_table, 'html.parser')
    # 提取并翻译表格的第一行（表头）
    original_header_row = soup.find('tr')
    translated_header_row = '<tr>'
    for cell in original_header_row.find_all('td'):
        original_text = cell.text.strip()
        translated_text = translate_model(original_text)[0]['translation_text']
        translated_text = my_utils.do_sentence(translated_text)
        translated_cell = f'<td>{translated_text}</td>'
        translated_header_row += translated_cell
    translated_header_row += '</tr>'

    # 提取并翻译表格的内容行
    translated_table = soup.new_tag('table')  # 创建一个新的表格用于存储翻译后的文本
    translated_table.append(BeautifulSoup(translated_header_row, 'html.parser'))  # 将翻译后的表头行插入到表格中
    for row in soup.find_all('tr')[1:]:
        translated_row = '<tr>'  # 创建新的行
        for cell in row.find_all('td'):
            original_text = cell.text.strip()  # 提取原始文本
            translated_text = translate_model(original_text)[0]['translation_text']
            translated_text = my_utils.do_sentence(translated_text)
            translated_cell = f'<td>{translated_text}</td>'  # 创建新的单元格
            translated_row += translated_cell  # 将单元格添加到行中
        translated_row += '</tr>'
        translated_table.append(BeautifulSoup(translated_row, 'html.parser'))  # 将行添加到表格中

    return translated_table
def img2doc(img,image_name,savePath):
    # 加载翻译模型
    #translate_model = hub.Module(name='transformer_zh-en', beam_size=5)
    # translate_model = hub.Module(name='baidu_translate')
    model_path = 'models/translate/en-zh'
    tokenizer = AutoTokenizer.from_pretrained(model_path)
    model = AutoModelForSeq2SeqLM.from_pretrained(model_path)
    translate_model = transformers.pipeline("translation", model=model, tokenizer=tokenizer)
    #table_engine = PPStructure(show_log=False, recovery=True, image_orientation=True, layout_model_dir="CDLA dict",structure_version="PP-StructureV2")
    table_engine = PPStructure(show_log=False,recovery=True, lang='en',cls_model_dir='models/ocr/cls/ch_ppocr_mobile_v2.0_cls_infer',det_model_dir='models/ocr/det/en/en_PP-OCRv4_det_infer',rec_model_dir='models/ocr/rec/en/en_PP-OCRv4_rec_infer',
                               layout_model_dir='models/ocr/layout/picodet_lcnet_x1_0_fgd_layout_infer',table_model_dir='models/ocr/table/en_ppstructure_mobile_v2.0_SLANet_infer')
    ocr = PaddleOCR(use_angle_cls=True, lang='en',cls_model_dir='models/ocr/cls/ch_ppocr_mobile_v2.0_cls_infer',det_model_dir='models/ocr/det/en/en_PP-OCRv4_det_infer',rec_model_dir='models/ocr/rec/en/en_PP-OCRv4_rec_infer')
    save_folder = os.path.join(savePath,'output/')
    result = table_engine(img)
    for i in range(len(result)):
        if result[i]["type"] != "figure" and result[i]["type"] != "table" and result[i]["type"] != "equation":
            # 本地中译英模型
            for j in result[i]["res"]:
                content = j["text"]
                translate_text = translate_model(content)[0]['translation_text']
                translate_text = my_utils.do_sentence(translate_text)
                j["text"] = translate_text
            # 百度模型英译中
                # content = j["text"]
                # translate_text = translate_model.translate(content)
                # j["text"] = translate_text
        elif result[i]["type"] == "figure":
            roi_img = result[i]["img"]
            res = ocr.ocr(roi_img, cls=True)
            for j in res[0]:
                content = j[1][0]
                translate_text = translate_model(content)[0]['translation_text']
                translate_text = my_utils.do_sentence(translate_text)
                text_region = j[0]
                roi_img = image_translate(roi_img,text_region, translate_text)
            result[i]['img'] = roi_img
        elif result[i]["type"] == "table":
            result[i]["res"]["html"] = "{}".format(table_translate(result[i]["res"]["html"],translate_model))
    save_structure_res(result, save_folder, image_name)
    h, w, _ = img.shape
    res = sorted_layout_boxes(result, w)
    convert_info_docx(img, res, save_folder, image_name)
    logger.info('已全部处理完成!')
if __name__ == '__main__':
    savePath = sys.argv[2]
    imgPath = sys.argv[1]
    img_name = os.path.basename(imgPath).split('.')[0]
    img = cv2.imread(imgPath)
    img2doc(img, img_name, savePath)