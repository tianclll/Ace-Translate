#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
REST API 端到端测试脚本 (AceTranslatePro)
==========================================

针对 AceTranslatePro 内嵌的 REST HTTP API 做端到端测试。

启动被测应用（设置 → REST API Server → 启用，或 config.json 中 api_enabled=true）后运行：
    python script/test_api.py
    python script/test_api.py --host 127.0.0.1 --port 18888
    python script/test_api.py --only health,kb,translate   # 只跑指定模块

使用 Python 标准库 urllib，无需安装任何第三方依赖。

退出码：
    0  = 全部通过
    1  = 存在失败用例
    2  = 无法连接服务器
"""

import argparse
import base64
import json
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

# ============================================================
# 配置
# ============================================================

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 18888
REQUEST_TIMEOUT = 30          # 普通请求超时（秒）
JOB_POLL_INTERVAL = 1.0       # 轮询 job 间隔（秒）
JOB_POLL_MAX = 120            # 最大轮询次数

PASS = 0
FAIL = 0
SKIP = 0
_checks = []                  # 收集每个用例结果


def report(name, ok, detail=""):
    """记录一个用例结果。"""
    global PASS, FAIL, SKIP
    status = "PASS" if ok else "FAIL"
    if ok:
        PASS += 1
    else:
        FAIL += 1
    line = f"[{status}] {name}"
    if detail:
        line += f" — {detail}" if status == "PASS" else f" :: {detail}"
    _checks.append(line)
    print(line)


def skip(name, reason):
    global SKIP
    SKIP += 1
    print(f"[SKIP] {name} — {reason}")


# ============================================================
# HTTP 客户端
# ============================================================

class ApiClient:
    """基于 urllib 的最小 HTTP 客户端。"""

    def __init__(self, base_url):
        self.base = base_url

    def request(self, method, path, body=None, query=None, expect_status=(200,)):
        """
        method: GET/POST/DELETE
        body: dict (会自动 JSON 编码) 或 None
        query: dict → 拼到 query string
        expect_status: 期望接受的 HTTP 状态码集合，若不在其中则抛异常
        """
        url = self.base + path
        if query:
            url += "?" + urllib.parse.urlencode(query)

        data = None
        headers = {}
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"

        req = urllib.request.Request(url, data=data, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
                code = resp.getcode()
                raw = resp.read()
        except urllib.error.HTTPError as e:
            code = e.code
            raw = e.read()

        if code not in expect_status:
            raise AssertionError(
                f"{method} {path} -> HTTP {code}, 期望 {expect_status}: {raw[:300]!r}")

        # 解析 JSON；空响应或无 JSON 则返回 None
        text = raw.decode("utf-8", errors="replace").strip()
        payload = None
        if text:
            try:
                payload = json.loads(text)
            except json.JSONDecodeError:
                payload = text
        return code, payload

    def get(self, path, query=None, **kw):
        return self.request("GET", path, query=query, **kw)

    def post(self, path, body=None, **kw):
        return self.request("POST", path, body=body, **kw)

    def delete(self, path, body=None, **kw):
        return self.request("DELETE", path, body=body, **kw)


def wait_for_job(client, job_id, timeout=JOB_POLL_MAX * JOB_POLL_INTERVAL):
    """轮询任务直到 completed/failed，或超时。返回 (job_dict)。"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        _, job = client.get(f"/api/jobs/{job_id}", expect_status=(200, 404))
        if job and isinstance(job, dict):
            status = job.get("status")
            if status in ("completed", "failed", "cancelled"):
                return job
        time.sleep(JOB_POLL_INTERVAL)
    return None


# ============================================================
# 各测试模块
# ============================================================

def test_health(client):
    print("\n===== 1. 健康检查 =====")
    _, payload = client.get("/api/health")
    report("GET /api/health", payload == {"status": "ok"}, repr(payload))

    _, payload = client.get("/api/status", expect_status=(200,))
    report("GET /api/status 返回基础信息",
           isinstance(payload, dict) and "port" in payload and "kb_doc_count" in payload,
           repr(payload)[:200])
    return payload


def test_translate_text(client):
    print("\n===== 2. 文本翻译（异步）=====")
    code, resp = client.post("/api/translate/text",
                             {"text": "Hello, how are you?", "target_language": "Chinese"},
                             expect_status=(202,))
    report("POST /api/translate/text 返回 202",
           code == 202 and resp.get("status") == "pending", repr(resp))

    job_id = resp.get("job_id")
    report("返回 job_id", bool(job_id), job_id or "缺失")

    job = wait_for_job(client, job_id)
    report("任务最终完成", job and job.get("status") == "completed",
           job.get("status") if job else "超时")
    if job and job.get("status") == "completed":
        result = job.get("result", "")
        report("翻译结果非空", bool(result), repr(result)[:80])

    # 缺必填字段 → 400
    code, err = client.post("/api/translate/text", {}, expect_status=(400,))
    report("缺少 text 字段返回 400", code == 400 and "error" in str(err),
           repr(err)[:120])


def test_translate_file(client):
    print("\n===== 3. 文件翻译（异步）=====")
    # 写一个临时 txt 文件
    with tempfile.NamedTemporaryFile(
            mode="w", suffix=".txt", prefix="apitest_", encoding="utf-8",
            delete=False) as f:
        f.write("The quick brown fox jumps over the lazy dog.\n"
                "Python is a powerful language for automation.\n")
        tmp_path = Path(f.name)
    tmp_dir = tmp_path.parent
    out_dir = tmp_dir / "apitest_out"
    out_dir.mkdir(exist_ok=True)
    out_md = out_dir / "translated.md"

    code, resp = client.post("/api/translate/file",
                             {"file_path": str(tmp_path), "target_language": "Chinese",
                              "output_path": str(out_md)},
                             expect_status=(202,))
    report("POST /api/translate/file 返回 202", code == 202, repr(resp))

    job = wait_for_job(client, resp.get("job_id"))
    report("文件翻译任务完成", job and job.get("status") == "completed",
           job.get("status") if job else "超时")
    if job and job.get("status") == "completed":
        out_path = job.get("result", "")
        report("输出文件已生成", bool(out_path) and Path(out_path).exists(), out_path)
        # 验证指定 output_path 生效：结果应为指定的路径
        report("output_path 生效（结果落在指定路径）",
               Path(out_path).resolve() == out_md.resolve(), f"{out_path} vs {out_md}")
        if out_path and Path(out_path).exists():
            Path(out_path).unlink()

    # 不存在的文件 → 400
    code, err = client.post("/api/translate/file",
                            {"file_path": "Z:/definitely/not/exists.docx"},
                            expect_status=(400,))
    report("不存在的文件返回 400", code == 400, repr(err)[:120])

    tmp_path.unlink()
    # 回收子目录
    if out_dir.exists():
        for p in out_dir.iterdir():
            try:
                p.unlink()
            except Exception:
                pass
        try:
            out_dir.rmdir()
        except Exception:
            pass


def test_kb(client):
    print("\n===== 4. 知识库 CRUD =====")
    # 创建
    code, created = client.post("/api/kb/entries",
                                {"title": "apitest.md", "file_type": "md",
                                 "file_size": 42,
                                 "markdown_content": "# API Test\n\ntest document"},
                                expect_status=(201,))
    report("POST /api/kb/entries 创建成功", code == 201 and created.get("id"), repr(created))
    entry_id = created.get("id")

    # 列表
    _, listing = client.get("/api/kb/entries", query={"limit": 10})
    report("GET /api/kb/entries 返回列表",
           isinstance(listing, dict) and "entries" in listing)
    ids = [e.get("id") for e in listing.get("entries", [])]
    report("新条目出现在列表中", entry_id in ids)

    # 单条查询
    _, one = client.get(f"/api/kb/entries/{entry_id}")
    report("GET /api/kb/entries/{id} 可查询", one.get("id") == entry_id, repr(one)[:120])

    # 搜索
    _, found = client.get("/api/kb/search", query={"q": "apitest"})
    report("GET /api/kb/search 命中", any(e.get("id") == entry_id
                                         for e in found.get("entries", [])),
           repr(found)[:160])

    # 标签
    _, tag = client.post("/api/kb/tags", {"name": "apitest_tag"}, expect_status=(201,))
    report("POST /api/kb/tags 创建标签", tag.get("id") is not None, repr(tag))
    tag_id = tag.get("id")

    # 术语
    _, term = client.post("/api/kb/glossary",
                          {"term": "API", "translation": "应用程序接口",
                           "source_lang": "English", "target_lang": "Chinese"},
                          expect_status=(201,))
    _, glossary = client.get("/api/kb/glossary")
    report("POST+GET /api/kb/glossary 术语可见",
           any(t.get("term") == "API" for t in glossary.get("terms", [])),
           repr(glossary)[:200])

    # 清理：删除术语、标签、条目
    if term and term.get("id"):
        client.delete(f"/api/kb/glossary/{term['id']}")
    if tag_id:
        client.delete(f"/api/kb/tags/{tag_id}")
    code, del_resp = client.delete(f"/api/kb/entries/{entry_id}")
    report("DELETE /api/kb/entries/{id} 删除成功",
           code == 200 and del_resp.get("deleted") == entry_id, repr(del_resp))

    # 删除后查询 → 404
    code, err = client.get(f"/api/kb/entries/{entry_id}", expect_status=(404,))
    report("删除后查询返回 404", code == 404, repr(err)[:120])


def test_translate_photo(client):
    print("\n===== 5. 图片翻译（base64，异步）=====")
    # 用 base64 编码一张最小 PNG
    png_b64 = (
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk"
        "YAAAAAYAAjCB0C8AAAAASUVORK5CYII="
    )
    code, resp = client.post("/api/translate/photo",
                             {"image_base64": png_b64, "target_language": "Chinese"},
                             expect_status=(202,))
    report("POST /api/translate/photo 返回 202", code == 202,
           f"code={code}, resp={resp!r}")

    job = wait_for_job(client, resp.get("job_id"))
    # 图片内容简单，OCR 可能为空，只要求任务有明确终态即可
    report("图片翻译任务进入终态",
           job is not None and job.get("status") in ("completed", "failed"),
           job.get("status") if job else "超时")


def test_asr(client):
    print("\n===== 6. 语音识别（ASR）=====")
    # Generate a minimal valid WAV file (16kHz 16-bit mono, 0.5s silence)
    import io, struct
    sample_rate = 16000
    num_samples = sample_rate // 2  # 0.5s
    pcm_data = b'\x00\x00' * num_samples  # silence

    # Build WAV header (44 bytes)
    wav_buf = io.BytesIO()
    wav_buf.write(b'RIFF')
    wav_buf.write(struct.pack('<I', 36 + len(pcm_data)))
    wav_buf.write(b'WAVE')
    wav_buf.write(b'fmt ')
    wav_buf.write(struct.pack('<IHHIIHH', 16, 1, 1, sample_rate, sample_rate * 2, 2, 16))
    wav_buf.write(b'data')
    wav_buf.write(struct.pack('<I', len(pcm_data)))
    wav_buf.write(pcm_data)
    wav_bytes = wav_buf.getvalue()
    wav_b64 = __import__('base64').b64encode(wav_bytes).decode()

    # 发送 base64 WAV 进行识别
    code, resp = client.post("/api/asr/recognize",
                             {"audio_base64": wav_b64, "max_duration": 10},
                             expect_status=(200,))
    report("POST /api/asr/recognize 返回 200", code == 200, repr(resp)[:200])
    if isinstance(resp, dict):
        report("返回 text 字段", "text" in resp)
        report("返回 duration_ms 字段", "duration_ms" in resp)

    # 缺少 audio_base64 → 400
    code, err = client.post("/api/asr/recognize", {}, expect_status=(400,))
    report("缺少 audio_base64 返回 400", code == 400, repr(err)[:120])

    # 数据过大 → 400
    code, err = client.post("/api/asr/recognize",
                            {"audio_base64": "A" * 21 * 1024 * 1024},
                            expect_status=(400,))
    report("音频超过 20MB 返回 400", code == 400, repr(err)[:120])

    # 无效 base64 → 400
    code, err = client.post("/api/asr/recognize",
                            {"audio_base64": "NOT_VALID_BASE64!!!"},
                            expect_status=(400,))
    report("无效 base64 返回 400", code == 400, repr(err)[:120])


def test_cancel_and_errors(client):
    print("\n===== 7. 取消与错误处理 =====")
    # 不存在 job → 404
    code, err = client.get("/api/jobs/not-a-real-job", expect_status=(404,))
    report("GET /api/jobs/{不存在} 返回 404", code == 404, repr(err)[:120])

    code, err = client.delete("/api/jobs/not-a-real-job/cancel", expect_status=(404,))
    report("DELETE 取消不存在的 job 返回 404", code == 404)

    # 非法 JSON body → 400 或 202（取决于是否识别）
    code, _ = client.post("/api/translate/photo", {}, expect_status=(400,))
    report("photo 缺少 image_base64 返回 400", code == 400)


# ============================================================
# 主流程
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="AceTranslatePro REST API 端到端测试")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"服务器地址 (默认 {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"端口 (默认 {DEFAULT_PORT})")
    parser.add_argument("--only", default="file",
                        help="逗号分隔指定模块: all,health,translate,kb,file,photo,asr,cancel")
    args = parser.parse_args()

    base = f"http://{args.host}:{args.port}"
    client = ApiClient(base)

    # 连通性检查
    try:
        client.get("/api/health", expect_status=(200,))
    except Exception as e:
        print(f"\n无法连接服务器 {base}: {e}")
        print("请先启动 AceTranslatePro，并在设置中启用 REST API（或 config.json 设 api_enabled=true）。")
        sys.exit(2)

    print(f"已连接 {base}，开始测试...")

    requested = {m.strip() for m in args.only.split(",") if m.strip()}

    if "all" in requested or "health" in requested:
        test_health(client)
    if "all" in requested or "translate" in requested or "text" in requested:
        test_translate_text(client)
    if "all" in requested or "translate" in requested or "file" in requested:
        test_translate_file(client)
    if "all" in requested or "kb" in requested:
        test_kb(client)
    if "all" in requested or "translate" in requested or "photo" in requested:
        test_translate_photo(client)
    if "all" in requested or "asr" in requested:
        test_asr(client)
    if "all" in requested or "cancel" in requested or "error" in requested:
        test_cancel_and_errors(client)

    # 汇总
    print("\n" + "=" * 50)
    print(f"结果汇总: 通过 {PASS} | 失败 {FAIL} | 跳过 {SKIP}")
    if FAIL:
        print("\n失败用例:")
        for c in _checks:
            if c.startswith("[FAIL]"):
                print(c)
    print("=" * 50)
    sys.exit(1 if FAIL else 0)


if __name__ == "__main__":
    main()
