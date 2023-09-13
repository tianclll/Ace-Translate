import re
from nltk import word_tokenize
import numpy as np
import jieba

def process_word(text):

    if re.search(r'[\u4e00-\u9fff]', text):  # 判断是否包含中文字符
        text = text.replace(' ', '')
        if len(text) > 1:
            result = re.sub(r'([\u4e00-\u9fff])\1+', r'\1', text)  # 替换连续的相同中文字为第一个中文字
            return result
        else:
            return text
    else:
        result = re.sub(r'([a-zA-Z])\1{2,}', r'\1\1', text)  # 替换连续的相同字母为第一个字母
        return result
def remove_last_punctuation(text):
    # 匹配最后一个标点符号的正则表达式
    pattern = r'[,.?;]$'
    punctuation_list = []
    cleaned_list = []
    for sentence in text:
        match = re.search(pattern, sentence)
        if match:
            # 如果匹配到了标点符号，则去除标点符号并记录到结果列表中
            cleaned_sentence = re.sub(pattern, '', sentence)
            punctuation = match.group()
        else:
            # 如果没有匹配到标点符号，则保持原始文本并将结果列表中添加空字符串
            cleaned_sentence = sentence
            punctuation = ''
        punctuation_list.append(punctuation)
        cleaned_list.append(cleaned_sentence)
    return punctuation_list, cleaned_list

def cosine_similarity(u, v):
    dot_product = np.dot(u, v)
    norm_u = np.linalg.norm(u)
    norm_v = np.linalg.norm(v)
    similarity = dot_product / (norm_u * norm_v)
    return similarity
def do_sentence(translated_text):
    translated_text = process_word(translated_text)
    print(translated_text)
    tokenized_sentence = list(jieba.cut(translated_text))
    # 保留第一个词语
    words = [tokenized_sentence[0]]  # 保留第一个词语
    for i in range(1, len(tokenized_sentence)):
        if tokenized_sentence[i] != tokenized_sentence[i - 1]:
            words.append(tokenized_sentence[i])
    words = [process_word(word) for word in words]
    print(words)
    filtered_sentence = "".join(words)
    sentences = []
    j = ''
    for i in words:
        j = j + i
        if i[-1] in [',', '?', '!', ')', ';', ' ', '...']:
            sentences.append(j)
            j = ''
    sentences.append(j)
    if len(sentences) <= 1:
        return filtered_sentence
    # 分词和构建词袋表示
    punctuation_list, sentences = remove_last_punctuation(sentences)
    print(punctuation_list)
    print(sentences)
    tokenized_sentences = [list(jieba.cut(sentence)) for sentence in sentences]
    vocabulary = set()
    for sentence in tokenized_sentences:
        vocabulary.update(sentence)
    vocabulary = list(vocabulary)
    word_to_index = {word: i for i, word in enumerate(vocabulary)}
    bag_of_words = np.zeros((len(sentences), len(vocabulary)))
    for i, sentence in enumerate(tokenized_sentences):
        for word in sentence:
            word_index = word_to_index[word]
            bag_of_words[i, word_index] += 1

    # 计算词频分布
    word_frequency = bag_of_words / np.linalg.norm(bag_of_words, axis=1, keepdims=True)

    # 保留第一个句子
    filtered_sentences = [sentences[0]+punctuation_list[0]]  # 保留第一个句子
    for i in range(1, len(sentences)):
        similarity = cosine_similarity(word_frequency[i-1], word_frequency[i])
        print(similarity)
        if similarity < 0.75:
            filtered_sentences.append(sentences[i]+punctuation_list[i])
            print(punctuation_list[i])

    # 输出结果

    sentence = ''.join(filtered_sentences)
    return sentence
if __name__ == '__main__':

    print(do_sentence("模块,模块,appple"))