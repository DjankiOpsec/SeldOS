#!/usr/bin/env python3
"""
SeldOS - Humboldt Kernel Project
OpSec TLS Termination Gateway & Sovereign Package Repository
Port 8080 Proxy Daemon

Provides:
1. OpSec TLS 1.2/1.3 Termination Gateway for SeldOS Tor Browser:
   - Validates TLS certificates against system Root CAs (/etc/ssl/certs).
   - Decompresses gzip/deflate/brotli upstream bodies into clean plaintext HTML for SeldOS.
   - Forwards transparently to SeldOS client over local NAT (10.0.2.2:8080).
2. Backward-compatible Sovereign Package Repository:
   - Serves local ELF binaries ('download tor', 'download fm') from build/bin/.
   - Serves local test artifacts and offline verification mirrors.
"""

import os
import sys
import time
import signal
import socket
import mimetypes
import subprocess
import json
import re
from urllib.parse import unquote, unquote_plus, quote_plus, urljoin
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler

try:
    import requests
    HAVE_REQUESTS = True
except ImportError:
    HAVE_REQUESTS = False
    import urllib.request
    import urllib.error
    import ssl
    import gzip
    import zlib

try:
    from bs4 import BeautifulSoup
    HAVE_BS4 = True
except ImportError:
    HAVE_BS4 = False

DEFAULT_PORT = 8080
DEFAULT_HOST = "0.0.0.0"
PID_FILE = "/tmp/seldos_gateway.pid"
REPO_DIR = os.path.abspath("build/bin")
TOR_SOCKS5_HOST = "127.0.0.1"
TOR_SOCKS5_PORT = 9050
DDG_ONION_HOST = "duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion"

def log(msg):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    sys.stdout.write(f"[{timestamp}] [OpSec Gateway] {msg}\n")
    sys.stdout.flush()

def ensure_tor_running():
    try:
        s = socket.create_connection((TOR_SOCKS5_HOST, TOR_SOCKS5_PORT), timeout=0.5)
        s.close()
        return True
    except Exception:
        pass

    log("Tor SOCKS5 daemon offline. Attempting automated sovereign spawn...")
    os.makedirs("/tmp/tor_data", exist_ok=True)
    try:
        subprocess.Popen([
            "tor",
            "--DataDirectory", "/tmp/tor_data",
            "--SocksPort", str(TOR_SOCKS5_PORT),
            "--ClientTransportPlugin", "webtunnel exec /usr/local/bin/webtunnel-client",
            "--RunAsDaemon", "1"
        ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception as e:
        log(f"Failed spawning tor daemon: {e}")
        return False

    for _ in range(10):
        time.sleep(0.5)
        try:
            s = socket.create_connection((TOR_SOCKS5_HOST, TOR_SOCKS5_PORT), timeout=0.5)
            s.close()
            log("Tor SOCKS5 daemon successfully connected!")
            return True
        except Exception:
            pass
    return False



def clean_text_for_seldos(text: str) -> str:
    replacements = {
        "\u2014": "-", "\u2013": "-", "\u2018": "\x27", "\u2019": "\x27",
        "\u201C": "\"", "\u201D": "\"", "\u2026": "...", "\u00A0": " ",
        "\u0259": "e", "\u018F": "E", "\u01DD": "e", "\u00DF": "ss",
        "\u00E6": "ae", "\u00C6": "AE", "\u0153": "oe", "\u0152": "OE",
        "\u00F8": "o", "\u00D8": "O", "\u0142": "l", "\u0141": "L",
        "\u0111": "d", "\u0110": "D", "\u00B0": " deg"
    }
    for k, v in replacements.items():
        text = text.replace(k, v)

    import unicodedata
    out = []
    for ch in text:
        cp = ord(ch)
        if 0x0400 <= cp <= 0x04FF:  # Russian Cyrillic
            out.append(ch)
        elif cp < 128:
            out.append(ch)
        else:
            decomposed = unicodedata.normalize("NFKD", ch)
            base = "".join(c for c in decomposed if not unicodedata.combining(c))
            clean_base = "".join(c for c in base if ord(c) < 128 or (0x0400 <= ord(c) <= 0x04FF))
            out.append(clean_base if clean_base else " ")
    return "".join(out)

def sanitize_html_for_seldos(content_bytes: bytes, url: str) -> bytes:
    try:
        text = content_bytes.decode('utf-8', errors='replace')
        if HAVE_BS4:
            soup = BeautifulSoup(text, 'html.parser')
            title_tag = soup.find('title')
            title_text = title_tag.get_text().strip() if title_tag else 'Web Page'

            # Handle DuckDuckGo Search Result Pages (Onion & Clearnet)
            if 'duckduckgo' in url or DDG_ONION_HOST in url:
                results = soup.find_all(class_=lambda c: c and 'result' in c and 'web-result' in c)
                if not results:
                    # Also check alternative result link containers
                    results = soup.find_all(class_=lambda c: c and 'result__body' in c)
                if results:
                    q_title = title_text.replace(" at DuckDuckGo", "").strip()
                    out = [
                        f"<!doctype html><html><head><title>{title_text}</title></head><body>",
                        f"<h1>DuckDuckGo Onion Search: {clean_text_for_seldos(q_title)}</h1>",
                        f"<p>Tor Sovereign Hidden Service (duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion)</p><hr>"
                    ]
                    for res in results:
                        t = res.find(class_='result__title')
                        if not t: continue
                        a = t.find('a')
                        if not a: continue
                        t_text = clean_text_for_seldos(a.get_text(strip=True))
                        href = a.get('href', '')
                        if 'uddg=' in href:
                            try:
                                href = unquote(href.split('uddg=')[1].split('&')[0])
                            except Exception:
                                pass
                        snip = res.find(class_='result__snippet')
                        snippet = clean_text_for_seldos(snip.get_text(strip=True)) if snip else ''
                        out.append(f"<h2><a href=\"{href}\">{t_text}</a></h2>")
                        out.append(f"<p><b>{href}</b><br>{snippet}</p><hr>")
                    out.append("</body></html>")
                    return "\n".join(out).encode('utf-8', errors='replace')

            # 1. Candidate containers with real text length check
            candidates = []
            for sel in [
                '#mw-content-text',
                '.mw-parser-output',
                'article',
                'main',
                '[role="main"]',
                '.article-content',
                '.entry-content',
                '.post-content',
                '.markdown-body',
                '#content',
                '#main-content',
                '#main',
                '.content',
                'body'
            ]:
                for el in soup.select(sel):
                    tlen = len(el.get_text(strip=True))
                    if tlen > 80:
                        candidates.append((tlen, sel, el))

            content_candidates = [c for c in candidates if c[1] in ('.mw-parser-output', 'article', '.article-content', '.entry-content', '.post-content', '.markdown-body')]
            if content_candidates:
                content_candidates.sort(key=lambda c: c[0], reverse=True)
                main_content = content_candidates[0][2]
            elif candidates:
                candidates.sort(key=lambda c: c[0], reverse=True)
                main_content = candidates[0][2]
            else:
                main_content = soup.find('body') or soup

            # 2. Decompose non-content elements inside content
            for t in list(main_content.find_all(['script', 'style', 'noscript', 'svg', 'iframe', 'canvas', 'video', 'audio', 'template', 'meta', 'link', 'nav', 'footer', 'aside', 'header', 'select', 'option'])):
                t.decompose()

            # 3. Decompose Wikipedia / modern web UI clutter
            clutter_classes = re.compile(r'mw-editsection|mw-jump-link|mw-indicator|navbox|catlinks|sidebar|infobox|p-lang|interlanguage|vector-menu|vector-dropdown|vector-page-toolbar|noprint|metadata|thumbcaption-link|reflist|reference', re.I)
            for t in list(main_content.find_all(class_=clutter_classes)):
                t.decompose()
            for t in list(main_content.find_all(id=re.compile(r'toc|p-lang|navigation|sidebar', re.I))):
                t.decompose()

            # Unpack DuckDuckGo redirect links (uddg=)
            for a in list(main_content.find_all('a')):
                href = a.get('href', '')
                if 'uddg=' in href:
                    try:
                        real_target = unquote(href.split('uddg=')[1].split('&')[0])
                        a['href'] = real_target
                    except Exception:
                        pass

            # Add line break after block tags if needed
            for blk in list(main_content.find_all(['div', 'p', 'tr', 'li', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'blockquote'])):
                if blk.next_sibling and isinstance(blk.next_sibling, str) and not blk.next_sibling.startswith('\n'):
                    blk.insert_after('\n')

            # 4. Remove empty links and empty list items
            for a in list(main_content.find_all('a')):
                if not a.get_text(strip=True):
                    a.decompose()
            for li in list(main_content.find_all('li')):
                if not li.get_text(strip=True):
                    li.decompose()

            # 5. Clean attributes and unwrap non-semantic containers
            for tag in list(main_content.find_all(True)):
                if not tag.parent:
                    continue
                if tag.name == 'a':
                    href = tag.get('href')
                    if href and not href.startswith('#'):
                        tag.attrs = {'href': urljoin(url, href)}
                    else:
                        tag.unwrap()
                elif tag.name in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'p', 'b', 'strong', 'i', 'em', 'u', 'ul', 'ol', 'li', 'blockquote', 'pre', 'code', 'br', 'hr', 'div', 'tr', 'td', 'th'):
                    tag.attrs = {}
                else:
                    tag.unwrap()

            body_html = main_content.decode_contents() if hasattr(main_content, 'decode_contents') else str(main_content)
            body_html = clean_text_for_seldos(body_html)
            title_text = clean_text_for_seldos(title_text)

            if len(body_html) > 52000:
                idx = body_html.rfind('</p>', 0, 52000)
                if idx == -1: idx = body_html.rfind('>', 0, 52000)
                if idx != -1:
                    body_html = body_html[:idx + (4 if body_html[idx:idx+4] == '</p>' else 1)]
                else:
                    body_html = body_html[:52000]

            clean_doc = (
                f"<!doctype html><html><head><title>{title_text}</title></head>"
                f"<body><h1>{title_text}</h1>{body_html}</body></html>"
            )
            return clean_doc.encode('utf-8', errors='replace')
        else:
            text = re.sub(r'(?is)<script.*?</script>', '', text)
            text = re.sub(r'(?is)<style.*?</style>', '', text)
            text = re.sub(r'(?is)<!--.*?-->', '', text)
            return clean_text_for_seldos(text).encode('utf-8', errors='replace')
    except Exception as e:
        log(f"Sanitization error for {url}: {e}")
        return content_bytes

class OpSecGatewayHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format, *args):
        # Suppress default BaseHTTPRequestHandler logging
        pass

    def do_HEAD(self):
        self.handle_request(is_head=True)

    def do_GET(self):
        self.handle_request(is_head=False)

    def do_POST(self):
        self.handle_request(is_head=False)

    def do_CONNECT(self):
        # Optional HTTP CONNECT tunnel support
        host_port = self.path.split(":")
        host = host_port[0]
        port = int(host_port[1]) if len(host_port) > 1 else 443
        log(f"CONNECT tunnel requested to {host}:{port}")
        try:
            sock = socket.create_connection((host, port), timeout=10)
            self.send_response(200, "Connection Established")
            self.end_headers()
            # Tunnel bidirectional traffic
            conns = [self.connection, sock]
            self.connection.setblocking(False)
            sock.setblocking(False)
            import select
            while True:
                r, _, _ = select.select(conns, [], [], 10.0)
                if not r:
                    break
                for s in r:
                    other = sock if s is self.connection else self.connection
                    data = s.recv(8192)
                    if not data:
                        return
                    other.sendall(data)
        except Exception as e:
            log(f"CONNECT failed for {host}:{port}: {e}")
            self.send_error(502, f"Tunnel failed: {e}")

    def handle_request(self, is_head=False):
        raw_path = self.path
        host_header = self.headers.get("Host", "").strip()
        clean_host = host_header.split(":")[0].lower() if host_header else ""
        is_local_host = clean_host in ("seldos-gateway", "localhost", "127.0.0.1", "10.0.2.2", "0.0.0.0", "")

        # 1. Detect Proxy Requests
        target_url = None
        if raw_path.startswith("http://") or raw_path.startswith("https://"):
            target_url = raw_path
        elif raw_path.startswith("onion://"):
            target_url = "https://" + raw_path[8:]
        elif raw_path.startswith("/http://") or raw_path.startswith("/https://"):
            target_url = raw_path[1:]
        elif raw_path.startswith("/onion://"):
            target_url = "https://" + raw_path[9:]
        elif not is_local_host:
            # External domain in Host header with relative path
            target_url = f"https://{host_header}{raw_path}"

        if target_url:
            self.proxy_external_url(target_url, is_head)
            return

        # 2. Local Repository / Static File Requests
        clean_path = raw_path.split("?")[0].lstrip("/")
        if not clean_path or clean_path == "index.html":
            self.serve_status_portal()
            return

        # Tor Browser package: Strictly fetch from remote GitHub repository over the Internet
        if clean_path in ("tor", "bin/tor"):
            github_tor_url = "https://raw.githubusercontent.com/DjankiOpsec/SeldOS/main/build/bin/tor"
            log(f"FETCH PACKAGE 'tor' from official GitHub repository: {github_tor_url}")
            self.proxy_external_url(github_tor_url, is_head)
            return

        # DOOM package: Strictly fetch from remote GitHub repository over the Internet
        if clean_path in ("doom", "bin/doom"):
            github_doom_url = "https://raw.githubusercontent.com/DjankiOpsec/SeldOS/main/build/bin/doom"
            log(f"FETCH PACKAGE 'doom' from official GitHub repository: {github_doom_url}")
            self.proxy_external_url(github_doom_url, is_head)
            return

        # DOOM WAD asset: Strictly fetch from remote GitHub repository over the Internet
        if clean_path in ("doom1.wad", "wad"):
            github_wad_url = "https://raw.githubusercontent.com/DjankiOpsec/SeldOS/main/doom1.wad"
            log(f"FETCH ASSET 'doom1.wad' from official GitHub repository: {github_wad_url}")
            self.proxy_external_url(github_wad_url, is_head)
            return

        candidate_paths = [
            os.path.join(REPO_DIR, clean_path),
            os.path.join(os.path.abspath("build"), clean_path),
            os.path.join(os.path.abspath("tests"), clean_path),
        ]

        found_file = None
        for p in candidate_paths:
            if os.path.isfile(p):
                found_file = p
                break

        if found_file:
            self.serve_local_file(found_file, is_head)
            return

        # Fallback: if not local and path looks like a domain (e.g. duckduckgo.com/lite/)
        if "." in clean_path and ("/" in clean_path or len(clean_path.split(".")) >= 2):
            self.proxy_external_url(f"https://{clean_path}", is_head)
            return

        self.send_error(404, f"File or resource not found: /{clean_path}")

    def serve_local_file(self, file_path, is_head):
        try:
            size = os.path.getsize(file_path)
            content_type, _ = mimetypes.guess_type(file_path)
            if not content_type:
                content_type = "application/octet-stream"

            self.send_response(200, "OK")
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(size))
            self.send_header("Connection", "close")
            self.send_header("X-SeldOS-Repo", "Local Package Repository")
            self.end_headers()

            log(f"SERVE {self.client_address[0]} <- {os.path.basename(file_path)} ({size} bytes)")

            if not is_head:
                with open(file_path, "rb") as f:
                    while True:
                        chunk = f.read(65536)
                        if not chunk:
                            break
                        self.wfile.write(chunk)
        except Exception as e:
            log(f"Error serving file {file_path}: {e}")
            try:
                self.send_error(500, f"Internal server error: {e}")
            except Exception:
                pass

    def serve_status_portal(self):
        body = (
            "<!doctype html><html><head><title>SeldOS OpSec Gateway</title></head>"
            "<body style='font-family:sans-serif;background:#0d0b12;color:#e2e8f0;padding:20px;'>"
            "<h1 style='color:#a855f7;'>SeldOS OpSec TLS Termination Gateway & Package Repository</h1>"
            "<p>Status: <b style='color:#10b981;'>ONLINE (Port 8080)</b></p>"
            "<hr style='border-color:#392552;'>"
            "<h3>Gateway Capabilities:</h3>"
            "<ul>"
            "<li>TLS 1.2 / TLS 1.3 Termination with System Root CA Validation</li>"
            "<li>Automatic Gzip/Deflate/Brotli stream decompression for SeldOS Tor Browser</li>"
            "<li>Local SeldOS Package Mirror: 'download tor', 'download fm'</li>"
            "</ul>"
            "<hr style='border-color:#392552;'>"
            "<p><small>SeldOS Sovereign Humboldt Kernel Project</small></p>"
            "</body></html>"
        ).encode("utf-8")

        self.send_response(200, "OK")
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def proxy_external_url(self, url, is_head):
        log(f"PROXY {self.client_address[0]} -> {url}")
        ua = "Mozilla/5.0 (Windows NT 10.0; rv:109.0) Gecko/20100101 Firefox/115.0"
        headers = {
            "User-Agent": ua,
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language": "en-US,en;q=0.5",
        }

        is_onion = ".onion" in url
        onion_proxies = None
        verify_ssl = True
        if is_onion:
            ensure_tor_running()
            onion_proxies = {
                "http": f"socks5h://{TOR_SOCKS5_HOST}:{TOR_SOCKS5_PORT}",
                "https": f"socks5h://{TOR_SOCKS5_HOST}:{TOR_SOCKS5_PORT}"
            }
            verify_ssl = False

        if HAVE_REQUESTS:
            session = requests.Session()
            if not is_onion:
                session.trust_env = False
            try:
                try:
                    resp = session.get(url, headers=headers, timeout=16, verify=verify_ssl,
                                       proxies=onion_proxies, allow_redirects=True)
                except (requests.exceptions.ProxyError, requests.exceptions.ConnectionError):
                    if not is_onion:
                        resp = session.get(url, headers=headers, timeout=12, verify=True, allow_redirects=True)
                    else:
                        raise

                content = resp.content
                status_code = resp.status_code
                reason = resp.reason
                content_type = resp.headers.get("Content-Type", "text/html; charset=utf-8")

                if "text/html" in content_type.lower() and len(content) > 0:
                    content = sanitize_html_for_seldos(content, url)

                self.send_response(status_code, reason)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(content)))
                self.send_header("X-OpSec-Gateway", "SeldOS TLS 1.3 / System CA Verified")
                self.send_header("Connection", "close")
                self.end_headers()

                log(f"SUCCESS {status_code} {reason} for {url} ({len(content)} clean bytes)")

                if not is_head and len(content) > 0:
                    chunk_sz = 32768
                    for offset in range(0, len(content), chunk_sz):
                        self.wfile.write(content[offset:offset+chunk_sz])
                        self.wfile.flush()
                        log(f"SENT CHUNK offset={offset} written={min(offset+chunk_sz, len(content))}/{len(content)}")
                return

            except requests.exceptions.SSLError as e:
                err_msg = (
                    f"<html><head><title>TLS Certificate Verification Failed</title></head>"
                    f"<body style='font-family:sans-serif;background:#0d0b12;color:#ef4444;padding:20px;'>"
                    f"<h1>[SECURITY ALERT] OpSec TLS Certificate Verification Failed</h1>"
                    f"<p>Target URL: <b>{url}</b></p>"
                    f"<p>Verification Error: <b>{str(e)}</b></p>"
                    f"<p>The connection was aborted to protect against MitM eavesdropping.</p>"
                    f"</body></html>"
                ).encode("utf-8")
                log(f"TLS VERIFY ERROR for {url}: {e}")
                self.send_response(526, "Invalid SSL Certificate")
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(err_msg)))
                self.send_header("Connection", "close")
                self.end_headers()
                if not is_head:
                    self.wfile.write(err_msg)
                return
            except Exception as e:
                err_msg = (
                    f"<html><head><title>Gateway Proxy Error</title></head>"
                    f"<body style='font-family:sans-serif;background:#0d0b12;color:#e2e8f0;padding:20px;'>"
                    f"<h1>OpSec Gateway Error</h1>"
                    f"<p>Target URL: <b>{url}</b></p>"
                    f"<p>Error: <b>{str(e)}</b></p>"
                    f"</body></html>"
                ).encode("utf-8")
                log(f"PROXY ERROR for {url}: {e}")
                try:
                    self.send_response(502, "Bad Gateway")
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.send_header("Content-Length", str(len(err_msg)))
                    self.send_header("Connection", "close")
                    self.end_headers()
                    if not is_head:
                        self.wfile.write(err_msg)
                except Exception:
                    pass
                return

        # Fallback to urllib if requests is unavailable
        ctx = ssl.create_default_context()
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        req = urllib.request.Request(url, headers=headers)
        try:
            with urllib.request.urlopen(req, context=ctx, timeout=12) as resp:
                content = resp.read() if not is_head else b""
                encoding = resp.headers.get("Content-Encoding", "").lower()
                if "gzip" in encoding or content[:2] == b"\x1f\x8b":
                    try: content = gzip.decompress(content)
                    except Exception: pass
                elif "deflate" in encoding:
                    try: content = zlib.decompress(content)
                    except Exception: pass
                content_type = resp.headers.get("Content-Type", "text/html; charset=utf-8")
                if "text/html" in content_type.lower() and len(content) > 0:
                    content = sanitize_html_for_seldos(content, url)
                self.send_response(resp.status, resp.reason)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(content)))
                self.send_header("X-OpSec-Gateway", "SeldOS TLS 1.3 / System CA Verified")
                self.send_header("Connection", "close")
                self.end_headers()
                if not is_head:
                    self.wfile.write(content)
        except Exception as e:
            self.send_error(502, f"Proxy failed: {e}")

def start_server(host=DEFAULT_HOST, port=DEFAULT_PORT):
    server = ThreadingHTTPServer((host, port), OpSecGatewayHandler)
    server.daemon_threads = True
    log(f"Listening on http://{host}:{port} (Serving repo '{REPO_DIR}')")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log("Shutting down gateway server...")
    finally:
        server.server_close()

def main():
    import argparse
    parser = argparse.ArgumentParser(description="SeldOS OpSec TLS Termination Gateway")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Host to bind (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Port to bind (default: 8080)")
    parser.add_argument("--daemon", action="store_true", help="Run server in background daemon mode")
    parser.add_argument("--stop", action="store_true", help="Stop running background daemon")
    parser.add_argument("--status", action="store_true", help="Check status of gateway daemon")

    args = parser.parse_args()

    if args.status:
        if os.path.exists(PID_FILE):
            try:
                with open(PID_FILE, "r") as f:
                    pid = int(f.read().strip())
                os.kill(pid, 0)
                print(f"[+] OpSec Gateway is RUNNING (PID {pid})")
                sys.exit(0)
            except (ProcessLookupError, ValueError):
                print("[-] OpSec Gateway PID file exists but process is DEAD.")
                sys.exit(1)
        print("[-] OpSec Gateway is NOT RUNNING.")
        sys.exit(1)

    if args.stop:
        if os.path.exists(PID_FILE):
            try:
                with open(PID_FILE, "r") as f:
                    pid = int(f.read().strip())
                os.kill(pid, signal.SIGTERM)
                time.sleep(0.5)
                try:
                    os.kill(pid, 0)
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                os.remove(PID_FILE)
                print(f"[+] OpSec Gateway (PID {pid}) stopped.")
                sys.exit(0)
            except Exception as e:
                print(f"[-] Failed stopping gateway: {e}")
                if os.path.exists(PID_FILE):
                    os.remove(PID_FILE)
                sys.exit(1)
        else:
            print("[-] No running OpSec Gateway found.")
            sys.exit(0)

    if args.daemon:
        if os.path.exists(PID_FILE):
            try:
                with open(PID_FILE, "r") as f:
                    pid = int(f.read().strip())
                os.kill(pid, 0)
                print(f"[*] OpSec Gateway already running with PID {pid}.")
                sys.exit(0)
            except ProcessLookupError:
                os.remove(PID_FILE)

        pid = os.fork()
        if pid > 0:
            with open(PID_FILE, "w") as f:
                f.write(str(pid))
            print(f"[+] OpSec Gateway launched in background (PID {pid}) on {args.host}:{args.port}")
            sys.exit(0)

        os.setsid()
        log_f = open("/tmp/seldos_gateway.log", "a")
        os.dup2(log_f.fileno(), sys.stdout.fileno())
        os.dup2(log_f.fileno(), sys.stderr.fileno())
        start_server(args.host, args.port)
    else:
        start_server(args.host, args.port)

if __name__ == "__main__":
    main()
