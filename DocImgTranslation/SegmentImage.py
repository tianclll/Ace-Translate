import cv2
import numpy as np
from FastSAM import Inference


def order_points(pts):
    # 一共4个坐标点
    rect = np.zeros((4, 2), dtype="float32")
    # 按顺序找到对应坐标0123分别是 左上，右上，右下，左下
    # 计算左上，右下
    s = pts.sum(axis = 1)
    rect[0] = pts[np.argmin(s)]
    rect[2] = pts[np.argmax(s)]
    # 计算右上和左下
    diff = np.diff(pts, axis = 1)
    rect[1] = pts[np.argmin(diff)]
    rect[3] = pts[np.argmax(diff)]
    return rect


def four_point_transform(image, pts):
    # 获取输入坐标点
    rect = order_points(pts)
    (tl, tr, br, bl) = rect

    # 计算输入的w和h值
    widthA = np.sqrt(((br[0] - bl[0]) ** 2) + ((br[1] - bl[1]) ** 2))
    widthB = np.sqrt(((tr[0] - tl[0]) ** 2) + ((tr[1] - tl[1]) ** 2))
    maxWidth = max(int(widthA), int(widthB))

    heightA = np.sqrt(((tr[0] - br[0]) ** 2) + ((tr[1] - br[1]) ** 2))
    heightB = np.sqrt(((tl[0] - bl[0]) ** 2) + ((tl[1] - bl[1]) ** 2))
    maxHeight = max(int(heightA), int(heightB))

    # 变换后对应坐标位置
    dst = np.array([
        [0, 0],
        [maxWidth - 1, 0],
        [maxWidth - 1, maxHeight - 1],
        [0, maxHeight - 1]], dtype="float32")

    # 计算变换矩阵
    M = cv2.getPerspectiveTransform(rect, dst)
    warped = cv2.warpPerspective(image, M, (maxWidth, maxHeight))
    # 返回变换后结果
    return warped


def image_segmentation(img_path):
    img = cv2.imread(img_path)
    ann = Inference.main(img_path)
    ann = ann.numpy()
    ann = ann.astype('uint8')
    # ann = np.reshape(ann, (ann.shape[1],ann.shape[2]))
    ann[ann == 1] = 255
    contours = []
    # print(ann.shape)
    for i in ann:
        # print(i.shape)
        kernel = np.ones((20, 20), np.uint8)
        i = cv2.erode(i, kernel, iterations=1)
        contour, hierarchy = cv2.findContours(i, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        contours.append(contour[0])
    docCnt = None
    if len(contours) > 0:
        # 根据轮廓大小进行排序
        cnts = sorted(contours, key=cv2.contourArea, reverse=True)
        # 遍历每一个轮廓
        for c in cnts:
            # 近似
            peri = cv2.arcLength(c, True)
            approx = cv2.approxPolyDP(c, 0.02 * peri, True)
            # 准备做透视变换
            if len(approx) == 4:
                if approx[0][0][0] == 0 and approx[0][0][1] == 0:
                    continue
                docCnt = approx
                break

    warped = four_point_transform(img, docCnt.reshape(4, 2))
    return warped
if __name__ == '__main__':
    image_segmentation("./16.jpg")
