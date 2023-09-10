import cv2
import numpy as np


# Gamma校正 fGamaa=0.45是常用值
def gamma_correction(src, fGamma):
    # 构建查找表
    lut = np.array([np.uint8(pow(i / 255.0, fGamma) * 255) for i in range(256)])
    dst = cv2.LUT(src, lut)
    return dst


def image_enhancement(image_data):
    # image_data = cv2.cvtColor(image_data, cv2.COLOR_BGR2GRAY)
    # 划分算法
    # 如果混合色与基色相同则结果色为白色
    # 如混合色为白色则结果色为基色不变
    # 如混合色为黑色则结果色为白色
    src = image_data.astype(np.float32) / 255.0
    gauss = cv2.GaussianBlur(src, (101, 101), 0)
    dst = src / gauss
    dst = np.clip(dst * 255, 0, 255).astype(np.uint8)

    # gamma变换
    matGamma = gamma_correction(dst.copy(), 1.5)

    # 显示最终结果
    cv2.imwrite("DocImgTranslation/imageEnhancement.jpg", matGamma)
    return matGamma

if __name__ == "__main__":
    P = cv2.imread('12.jpg', cv2.IMREAD_COLOR)
    gray_P = cv2.cvtColor(P, cv2.COLOR_BGR2GRAY)
    P_shape = gray_P.shape

    image_enhancement(gray_P)

