import sys
import os
import json
import argparse
from pathlib import Path

# 当打包成 EXE 时，将临时解压目录添加到 sys.path
if getattr(sys, 'frozen', False):
    # 运行时，sys._MEIPASS 指向临时目录
    base_path = sys._MEIPASS
else:
    # 开发模式，使用当前目录
    base_path = os.path.dirname(os.path.abspath(__file__))

# 将 base_path 添加到 sys.path，以便找到复制进去的库（docx、pptx 等目录）
if base_path not in sys.path:
    sys.path.insert(0, base_path)

# 确保项目根目录（包含 office2md 子包）也在 sys.path 中
project_root = Path(__file__).parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

# 现在可以安全导入 office2md.core
from office2md.core import convert

def main():
    parser = argparse.ArgumentParser(description="Convert Office files to Markdown")
    parser.add_argument("--input", required=True, help="输入文件路径")
    parser.add_argument("--output", help="输出 Markdown 文件路径（可选，默认在输入文件同目录生成同名 .md）")
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    if args.output:
        output_path = Path(args.output)
    else:
        output_path = input_path.parent / f"{input_path.stem}.md"

    try:
        result = convert(input_path, output=output_path)
        print(json.dumps({
            "status": "success",
            "markdown": str(output_path),
            "image_count": len(result.images),
            "metadata": result.metadata
        }))
    except Exception as e:
        print(json.dumps({"status": "error", "message": str(e)}), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()