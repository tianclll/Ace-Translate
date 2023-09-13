import transformers
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
def translate(text):
    model_path = 'models/translate/zh-en'
    tokenizer = AutoTokenizer.from_pretrained(model_path)
    translate_model = AutoModelForSeq2SeqLM.from_pretrained(model_path)
    pipeline = transformers.pipeline("translation", model=translate_model, tokenizer=tokenizer)
    translate_text = pipeline(text)[0]['translation_text']
    return translate_text