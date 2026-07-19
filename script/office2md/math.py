# math.py
# 封装 omml.py 中的转换函数，并提供 paragraph_has_math 等辅助函数

from lxml import etree
from .omml import oMath2Latex

_M = "{http://schemas.openxmlformats.org/officeDocument/2006/math}"

def paragraph_has_math(para_elem):
    """检查段落元素是否包含 OMML 公式"""
    return para_elem.find(f".//{_M}oMath") is not None

def extract_math_from_paragraph(para_elem):
    """从段落元素中提取所有公式，返回 LaTeX 字符串列表"""
    results = []
    for omath in para_elem.findall(f".//{_M}oMath"):
        try:
            latex = str(oMath2Latex(omath))
            if latex:
                results.append(latex)
        except Exception:
            continue
    return results

def convert_omath(omath_elem):
    """将单个 oMath 元素转换为 LaTeX 字符串"""
    try:
        return str(oMath2Latex(omath_elem))
    except Exception:
        return ""