
import transformers
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import my_utils
def translate(text):
    model_path = 'models/translate/en-zh'
    tokenizer = AutoTokenizer.from_pretrained(model_path)
    translate_model = AutoModelForSeq2SeqLM.from_pretrained(model_path)
    pipeline = transformers.pipeline("translation", model=translate_model, tokenizer=tokenizer)
    translate_text = pipeline(text)[0]['translation_text']
    translate_text = my_utils.do_sentence(translate_text)
    return translate_text