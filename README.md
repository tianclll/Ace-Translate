<p align="center">
<img src="image/translate.png" style="width:100px;" />
</p>
<h1 align="center">Ace-Translate</h1>
<p align="center">
一款本地离线的翻译程序
</p>



<p align="center">
  <img src="https://img.shields.io/badge/Ace%20Translate-pink">
  <img src="https://img.shields.io/badge/paddlepaddle-v2.4.0-blue">
  <img src="https://img.shields.io/badge/torch-v2.0.1-blue">
</p>




## Features

支持多种翻译场景。

- 汉译英和英译汉。
- 文本翻译
- 划词翻译
- 截图翻译
- 文件翻译，包括TXT文件、Excel、PPT、PDF、文档图片和Word
- 文档图片翻译



## INSTALL

推荐使用`python3.8`+`paddlepaddle2.4.0`+`torch2.0.1`

### 1.拉代码

```
git clone https://github.com/tianclll/Ace-Translate.git
```

```
cd Ace-Translate
```



### 2.安装

#### 2.1安装PaddlePaddle

- GPU

  ```
  python3 -m pip install paddlepaddle-gpu -i https://mirror.baidu.com/pypi/simple
  ```

- CPU

  ```
  python3 -m pip install paddlepaddle -i https://mirror.baidu.com/pypi/simple
  ```

#### 2.2安装依赖

```
pip install -r requirements.txt
```

#### 2.3下载模型文件

点击[此处](https://www.123pan.com/s/knrdjv-AC5N3.html)下载

下载放入项目文件夹(Ace-Translate)中。

#### 2.4安装Pyaudio
需要运行语音翻译才安装

- Linux

  ```
  sudo apt-get install libasound2-dev
  weget https://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz
  tar -xvf pa_stable_v190700_20210406.tgz
  cd portaudio
  ./configure
  make
  sudo make install
  make clean
  pip install pyaudio
  ```
- Mac

  ```
  sudo brew install libasound2-dev
  weget https://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz
  tar -xvf pa_stable_v190700_20210406.tgz
  cd portaudio
  ./configure
  make
  sudo make install
  make clean
  pip install pyaudio
  ```
- Windows
  ```
  pip install pipwin
  pipwin install pyaudio
  ```
#### 2.5安装ImageMagick

需要运行视频翻译才安装

### 3.设置

修改`config.conf`文件:

- 设置快捷键
- 设置运行设备 `gpu` or `cpu`

### 4.运行

注意：第一次运行各个模块都需要连网，下载相应模型

```
python main.py
```



## 效果展示

有"划词翻译","截图翻译","PDF翻译","文档图片翻译"四个功能,项目运行后会挂载到状态栏上，点击"x"时不会退出只是隐藏，点击状态栏上的"打开",就会弹出，点击状态栏上的"退出",才是真正的退出程序。

- 文本翻译

![Xnip2023-09-10_23-21-12](image/Xnip2023-09-10_23-21-12.jpg)

- 划词翻译

  - 选择"汉译英"或者"英译汉"，然后点击开始

  - 然后鼠标选中想要翻译的内容，点击复制
  - 按下设置的快捷键，就能翻译了

![Xnip2023-09-10_23-22-59](image/Xnip2023-09-10_23-22-59.jpg)



- 截图翻译

![Xnip2023-09-10_23-25-36](image/Xnip2023-09-10_23-25-36.jpg)

- 语音翻译

  支持`音频文件`和`语音录入`

  ![Xnip2023-09-10_23-26-39](image/Xnip2023-09-10_23-26-39.jpg)

- 视频翻译

​		支持`输出srt字幕文件`和`视频`

![Xnip2023-09-10_23-30-04](image/Xnip2023-09-10_23-30-04.jpg)

![Xnip2023-09-10_23-30-57](image/Xnip2023-09-10_23-30-57.jpg)

- 文件翻译

  - TXT

  ![Xnip2023-09-10_23-53-20](image/Xnip2023-09-10_23-53-20.jpg)

  ![Xnip2023-09-10_23-54-02](image/Xnip2023-09-10_23-54-02.jpg)

  - PDF

  ![Xnip2023-09-10_23-50-55](image/Xnip2023-09-10_23-50-55.jpg)

  ![Xnip2023-09-10_23-41-39](image/Xnip2023-09-10_23-41-39.jpg)

  - Excel

  ![Xnip2023-09-11_00-07-27](image/Xnip2023-09-11_00-07-27.jpg)

  ![Xnip2023-09-11_00-09-16](image/Xnip2023-09-11_00-09-16.jpg)

  - Word

  ![Xnip2023-09-11_00-00-42](image/Xnip2023-09-11_00-00-42.jpg)

  ![Xnip2023-09-11_00-03-44](image/Xnip2023-09-11_00-03-44.jpg)

  - Image

  类似PDF

- 文档图片翻译

![12](image/12.jpg)

![Xnip2023-09-10_23-41-39](image/Xnip2023-09-10_23-41-39.jpg)

## LICENSE
[Apache](https://github.com/tianclll/Ace-Translate/blob/main/LICENSE)

Copyright (c) 2023 tianclll
