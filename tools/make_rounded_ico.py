from PIL import Image, ImageDraw

# 打开 LOGO.png
img = Image.open("D:/AceTranslatePro/AceTranslatePro/ui/icons/LOGO.png").convert("RGBA")
w, h = img.size

# 计算圆角参数
r = int(min(w, h) * 0.2)

# 创建圆角遮罩
mask = Image.new("L", (w, h), 0)
draw = ImageDraw.Draw(mask)
draw.rounded_rectangle([(0, 0), (w-1, h-1)], radius=r, fill=255)

# 应用遮罩
result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
result.paste(img, (0, 0), mask)

# 保存为各种尺寸的 ICO
sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]

result.save("D:/AceTranslatePro/AceTranslatePro/ui/icons/LOGO.ico",
            format="ICO",
            sizes=sizes)

print(f"已生成圆角 ICO: {w}x{h}, 圆角半径={r}")
