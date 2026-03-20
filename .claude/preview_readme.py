import http.server
import markdown
import os
import mimetypes

PORT = 8765
REPO_ROOT = os.path.dirname(os.path.dirname(__file__))
README_PATH = os.path.join(REPO_ROOT, "README.md")

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/" or self.path == "/index.html":
            with open(README_PATH, "r") as f:
                md_content = f.read()
            html_body = markdown.markdown(md_content, extensions=["tables", "fenced_code"])
            html = f"""<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<title>README Preview</title>
<style>
body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif; max-width: 900px; margin: 40px auto; padding: 0 20px; line-height: 1.6; color: #24292e; }}
h1 {{ border-bottom: 1px solid #eaecef; padding-bottom: .3em; }}
h2 {{ border-bottom: 1px solid #eaecef; padding-bottom: .3em; }}
code {{ background: #f6f8fa; padding: 2px 6px; border-radius: 3px; font-size: 85%; }}
pre {{ background: #f6f8fa; padding: 16px; border-radius: 6px; overflow-x: auto; }}
pre code {{ background: none; padding: 0; }}
table {{ border-collapse: collapse; width: 100%; margin: 16px 0; }}
th, td {{ border: 1px solid #dfe2e5; padding: 6px 13px; }}
th {{ background: #f6f8fa; }}
img {{ max-width: 100%; border-radius: 6px; margin: 16px 0; }}
a {{ color: #0366d6; text-decoration: none; }}
hr {{ border: none; border-top: 1px solid #eaecef; margin: 24px 0; }}
</style>
</head><body>{html_body}</body></html>"""
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html.encode())
        else:
            # Serve static files from repo root
            file_path = os.path.join(REPO_ROOT, self.path.lstrip("/"))
            if os.path.isfile(file_path):
                mime_type, _ = mimetypes.guess_type(file_path)
                with open(file_path, "rb") as f:
                    data = f.read()
                self.send_response(200)
                self.send_header("Content-Type", mime_type or "application/octet-stream")
                self.end_headers()
                self.wfile.write(data)
            else:
                self.send_response(404)
                self.end_headers()

    def log_message(self, format, *args):
        pass

print(f"Serving README preview on http://localhost:{PORT}")
http.server.HTTPServer(("", PORT), Handler).serve_forever()
