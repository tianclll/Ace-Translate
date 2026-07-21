import os
import argparse
from structure import Structure
from imageEnhancement import image_enhancement
from SegmentImage import image_segmentation
from paddleocr.ppocr.utils.logging import get_logger

logger = get_logger()
ap = argparse.ArgumentParser()
ap.add_argument("-i", "--image", help="path to the input image")
ap.add_argument("-d","--image_dir")
ap.add_argument("-o","--output")
args = vars(ap.parse_args())

if args['image'] != None and args['image_dir'] == None:
    logger.info('开始分割图像...')
    img = image_segmentation(args['image'])
    #img = cv2.imread(args['image'],0)
    logger.info('分割图像已完成!')
    logger.info('开始图像增强...')
    img = image_enhancement(img)
    logger.info('图像增强已完成!')
    logger.info('开始版面分析并转为Word...')
    Structure(img,args['image'],args['output'],"en2ch")
    logger.info('已处理完成!')
elif args['image'] == None and args['image_dir'] != None:
    image_dir = args['image_dir']
    if os.path.exists(image_dir):
        contents = os.listdir(image_dir)
        for i, image in enumerate(contents):
            if image[-3:] not in ['jpg', 'JPG', 'png', 'PNG', 'peg']:
                contents.pop(i)
        logger.info('检测到{}张图片!'.format(len(contents)))
        for i,image in enumerate(contents):
            logger.info('正在处理第{}张图片...'.format(i+1))
            # image = cv2.imread(os.path.join(image_dir,i))
            logger.info('开始分割图像...')
            img = image_segmentation(os.path.join(image_dir,image))
            logger.info('分割图像已完成!')
            logger.info('开始图像增强...')
            img = image_enhancement(img)
            logger.info('图像增强已完成!')
            logger.info('开始版面分析并转为Word...')
            Structure(img, os.path.join(image_dir,image),args['output'])
            logger.info('第{}张图片已完成!'.format(i+1))
            print()
            if i == len(contents)-1:
                logger.info('已全部处理完成!')
                break
    else:
        print('请输入正确的路径!!!')

