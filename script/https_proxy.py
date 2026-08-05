#!/usr/bin/env python3
"""
本地 HTTPS 反向代理：手机浏览器 → AceTranslatePro REST API
解决浏览器 getUserMedia 需要 HTTPS 的问题。

用法：
    python311 script/https_proxy.py
    python311 script/https_proxy.py --port 8443 --api 127.0.0.1:18888

然后手机浏览器访问：https://<电脑IP>:8443/

首次运行会生成自签名证书（ssl/cert.pem, ssl/key.pem），
手机访问时需手动信任该证书（或添加安全例外）。
"""

import argparse
import http.server
import ipaddress
import os
import socket
import ssl
import sys
import threading
import urllib.request
from pathlib import Path

DEFAULT_PORT = 8443
DEFAULT_API = "http://127.0.0.1:18888"
SSL_DIR = Path(__file__).resolve().parent.parent / "webapp" / "ssl"
CERT_FILE = SSL_DIR / "cert.pem"
KEY_FILE = SSL_DIR / "key.pem"


def get_local_ip():
    """获取本机局域网 IP"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def generate_self_signed_cert(cert_path, key_path):
    """生成自签名证书（用于本地开发）"""
    try:
        from cryptography import x509
        from cryptography.x509.oid import NameOID
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.asymmetric import rsa
        import datetime
    except ImportError:
        print("需要安装 cryptography 库来生成证书：")
        print("  pip install cryptography")
        sys.exit(1)

    print("生成自签名证书...")
    SSL_DIR.mkdir(exist_ok=True)

    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    subject = issuer = x509.Name([
        x509.NameAttribute(NameOID.COUNTRY_NAME, "CN"),
        x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, "Local"),
        x509.NameAttribute(NameOID.LOCALITY_NAME, "Local"),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, "AceTranslatePro"),
        x509.NameAttribute(NameOID.COMMON_NAME, "localhost"),
    ])
    cert = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(issuer)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(datetime.datetime.utcnow())
        .not_valid_after(datetime.datetime.utcnow() + datetime.timedelta(days=365))
        .add_extension(x509.SubjectAlternativeName([
            x509.IPAddress(ipaddress.IPv4Address("127.0.0.1")),
        ]), critical=False)
        .sign(key, hashes.SHA256())
    )

    cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    key_path.write_bytes(key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption(),
    ))
    print(f"证书已生成：{cert_path}")
    print(f"私钥已生成：{key_path}")


class ProxyHandler(http.server.BaseHTTPRequestHandler):
    api_base = DEFAULT_API

    def log_message(self, format, *args):
        pass  # 静默日志

    def _proxy(self, method):
        # 构建目标 URL
        target = self.api_base + self.path
        try:
            req = urllib.request.Request(target, method=method)
            if method in ("POST", "PUT", "DELETE"):
                content_len = int(self.headers.get("Content-Length", 0))
                body = self.rfile.read(content_len) if content_len > 0 else None
                if body:
                    req.data = body
                # 转发 Content-Type
                ct = self.headers.get("Content-Type")
                if ct:
                    req.add_header("Content-Type", ct)

            with urllib.request.urlopen(req, timeout=30) as resp:
                status = resp.getcode()
                data = resp.read()
                self.send_response(status)
                # CORS 头
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
                self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
                # 转发 Content-Type
                ct = resp.headers.get("Content-Type")
                if ct:
                    self.send_header("Content-Type", ct)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

        except urllib.error.HTTPError as e:
            self.send_response(e.code)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(e.read())))
            self.end_headers()
            self.wfile.write(e.read())
        except Exception as e:
            self.send_response(502)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(f'{{"error": "Proxy error: {e}"}}'.encode())

    def do_GET(self):
        self._proxy("GET")

    def do_POST(self):
        self._proxy("POST")

    def do_DELETE(self):
        self._proxy("DELETE")

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.send_header("Content-Length", "0")
        self.end_headers()


def main():
    parser = argparse.ArgumentParser(description="AceTranslatePro HTTPS Proxy")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"HTTPS 端口（默认 {DEFAULT_PORT}）")
    parser.add_argument("--api", default=DEFAULT_API, help=f"API 地址（默认 {DEFAULT_API}）")
    parser.add_argument("--no-ssl", action="store_true", help="不使用 HTTPS（仅用于调试）")
    args = parser.parse_args()

    ProxyHandler.api_base = args.api

    # 检查 API 是否可达
    try:
        urllib.request.urlopen(args.api + "/api/health", timeout=3)
    except Exception:
        print(f"警告：无法连接 API 服务器 {args.api}")
        print("请确保 AceTranslatePro 已启动并启用 REST API")

    server = http.server.HTTPServer(("0.0.0.0", args.port), ProxyHandler)

    if not args.no_ssl:
        # 检查或生成证书
        if not CERT_FILE.exists() or not KEY_FILE.exists():
            generate_self_signed_cert(CERT_FILE, KEY_FILE)

        # 包装为 HTTPS
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(str(CERT_FILE), str(KEY_FILE))
        server.socket = context.wrap_socket(server.socket, server_side=True)
        scheme = "https"
    else:
        scheme = "http"

    ip = get_local_ip()
    print(f"=" * 50)
    print(f"AceTranslatePro HTTPS Proxy")
    print(f"=" * 50)
    print(f"API 后端：  {args.api}")
    print(f"代理地址：  {scheme}://0.0.0.0:{args.port}")
    print(f"手机访问：  {scheme}://{ip}:{args.port}/")
    print(f"")
    if not args.no_ssl:
        print(f"注意：使用自签名证书，手机访问时需添加安全例外")
        print(f"证书路径：  {CERT_FILE}")
    print(f"按 Ctrl+C 停止")
    print(f"=" * 50)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n代理已停止")
        server.server_close()


if __name__ == "__main__":
    main()
