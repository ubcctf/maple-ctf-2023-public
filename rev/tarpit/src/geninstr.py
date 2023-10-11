import struct
instructions = bytearray()

INC   = 0b000
DJZ   = 0b001
LDR   = 0b010
HALT  = 0b100

A = 0
B = 1

fmt = struct.Struct("<Bhh")

def djz(reg, zero_idx, next_idx: int):
    instructions.extend(fmt.pack(((DJZ << 2) | (reg << 1)), zero_idx, next_idx))

def p_instr(op, rx, ry, next_idx):
    instructions.extend(fmt.pack(((op << 2) | (rx << 1) | (ry)), 0, next_idx))

def inc(reg, nidx):
    return p_instr(INC, reg, 0, nidx)

def ldr(rx, ry, nidx):
    return p_instr(LDR, rx, ry, nidx)

def halt():
    instructions.extend(fmt.pack((HALT << 2), 0, 0))
 
flag = "maple{m1n5ky_m4ch1n35_4r3_c00l}"
ln = len(flag)

instr_ctr = 1
# encode input in primes
for i in range(len(flag)):
    for j in range(i): # increment 0 to next char
        inc(A, instr_ctr)
        instr_ctr += 1
    ldr(A, A, instr_ctr) # load next char
    instr_ctr += 1
    cr = flag[i]
    # decrement to zero, jumping past prime encoding if char is zero to early
    sp = instr_ctr -1 
    for j in range(ord(cr)):
        djz(A, zero_idx= sp + ord(cr) + 3 , next_idx=instr_ctr)
        instr_ctr += 1
    djz(A, zero_idx= instr_ctr , next_idx=instr_ctr + 1)
    instr_ctr += 1
    inc(B, instr_ctr)
    instr_ctr += 1
    # loop to exhaust input value
    djz(A, zero_idx= instr_ctr, next_idx=instr_ctr-1)
    instr_ctr += 1
    
halt()


# write to file "instructions"
with open("instructions", "w") as f:
    f.write("".join(["0x"+hex(i)[2:].zfill(2)+", " for i in instructions]))
