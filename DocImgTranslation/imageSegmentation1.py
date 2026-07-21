#!/usr/bin/env python
import requests
import json
import base64
import numpy as np
import cv2
def get_file_content(filePath):
    with open(filePath, 'rb') as fp:
        return fp.read()

class CommonOcr(object):
    def __init__(self, img_path):
        # 请登录后前往 “工作台-账号设置-开发者信息” 查看 x-ti-app-id
        # 示例代码中 x-ti-app-id 非真实数据
        self._app_id = 'a1ad6dc8a0a9f5da5487cb5a1a6e5bcf'
        # 请登录后前往 “工作台-账号设置-开发者信息” 查看 x-ti-secret-code
        # 示例代码中 x-ti-secret-code 非真实数据
        self._secret_code = '9ec1a530b4c985ed431891ca83e19acd'
        self._img_path = img_path

    def recognize(self):
        # 文档图像切边矫正
        url = 'https://api.textin.com/ai/service/v1/dewarp'
        head = {}
        try:
            image = get_file_content(self._img_path)
            head['x-ti-app-id'] = self._app_id
            head['x-ti-secret-code'] = self._secret_code
            result = requests.post(url, data=image, headers=head)
            return result.text
        except Exception as e:
            return e
def image_segmentation(image_path):
    response = CommonOcr(image_path)
    result = response.recognize()
    result = json.loads(result)["result"]["image"]
    img_binary = base64.b64decode(result)
    # Convert binary to numpy array
    img_np = np.fromstring(img_binary, np.uint8)
    # Convert numpy array to image
    img = cv2.imdecode(img_np, cv2.IMREAD_COLOR)
    return img
if __name__ == "__main__":
    response = CommonOcr(r'/Users/liuhongdi/计算机设计大赛/OCR图片文字识别/数据/JPG/000022.JPG')
    result = response.recognize()
    result = json.loads(result)["result"]["image"]

    img_binary = base64.b64decode(result)

    # Convert binary to numpy array
    img_np = np.fromstring(img_binary, np.uint8)

    # Convert numpy array to image
    img = cv2.imdecode(img_np, cv2.IMREAD_COLOR)