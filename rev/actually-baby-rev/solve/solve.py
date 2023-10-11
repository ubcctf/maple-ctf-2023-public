from pwn import *

HOST = 'actually-baby-rev.ctf.maplebacon.org'
PORT = 1337

p = remote(HOST, PORT)

p.clean()
p.sendline((b"5" * 15) + (b"13" * 13))
p.clean()
p.sendline(b"2444")
p.clean()
p.sendline(b"80673")
info(p.clean().decode())
info("FLAG FOUND")