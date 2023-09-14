
import os
import sys
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import my_utils
import transformers
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
if __name__ == '__main__':
    savePath = sys.argv[2]
    # 1、excel地址
    txtPath = sys.argv[1]
    model_path = 'models/translate/en-zh'
    tokenizer = AutoTokenizer.from_pretrained(model_path)
    model = AutoModelForSeq2SeqLM.from_pretrained(model_path)
    translate_model = transformers.pipeline("translation", model=model, tokenizer=tokenizer)
    with open(txtPath,'r') as f:
        text = f.readlines()
        f.close()

    lines = []
    for line in text:
        line_clean = line.replace('\n','')
        translate_line = translate_model(line_clean)[0]['translation_text']
        translate_line = my_utils.do_sentence(translate_line)
        translate_line = translate_line + '\n'
        lines.append(translate_line)

    txt_name = os.path.basename(txtPath)
    with open(os.path.join(savePath,txt_name),'w') as f:
        f.writelines(lines)
        f.close()