import os
import pandas as pd
import sys
sys.path.append(os.getcwd())
import my_utils
from bs4 import BeautifulSoup
import transformers
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
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
if __name__ == '__main__':
    savePath = sys.argv[2]
    # 1、excel地址
    excelPath = sys.argv[1]
    model_path = 'models/translate/en-zh'
    tokenizer = AutoTokenizer.from_pretrained(model_path)
    model = AutoModelForSeq2SeqLM.from_pretrained(model_path)
    translate_model = transformers.pipeline("translation", model=model, tokenizer=tokenizer)
    # 读取 Excel 文件
    df = pd.read_excel(excelPath, sheet_name=0, header=None)
    html = df.to_html(index=False)
    html_string = table_translate(html,translate_model)
    # 创建 HTML 模板
    html_template = f"""
    <html>
      <head>
        <title>Table HTML Page</title>
      </head>
      <body>
        {html_string}
      </body>
    </html>
    """

    with open("FileTranslation/Excel/tmp.html", "w") as file:
        file.write(html_template)
    # 使用 pandas 读取表格
    df = pd.read_html("FileTranslation/Excel/tmp.html", encoding='utf-8')[0]
    print(df)
    # 将数据框保存为 Excel 文件
    excel_name = os.path.basename(excelPath)
    df.to_excel(os.path.join(savePath, excel_name), index=False, header=None)