# https://blog.securityevaluators.com/xorshift128-backward-ff3365dc0c17
# modified to account for loss of precision in the floating point nums we receive
# uses unsat condition to check where boundary of LIFO xorshift128+ cache is
import sys
import math
import struct
import random
from z3 import *

MASK = 0xFFFFFFFFFFFFFFFF
PRECISION = 42

def float_to_bin_64(num):
    bin_repr = struct.pack('>d', num)
    bin_repr = "".join([f"{byte:08b}" for byte in bin_repr])
    return bin_repr

def float_get_lower_upper_bound(num):
    bin_repr = float_to_bin_64(num)
    known = bin_repr[:PRECISION]
    lower = known + '0' * (64 - PRECISION)
    upper = known + '1' * (64 - PRECISION)
    # interpret as 8 byte float
    lower = struct.unpack('>d', struct.pack('>Q', int(lower, 2)))[0]
    upper = struct.unpack('>d', struct.pack('>Q', int(upper, 2)))[0]
    return lower, upper

# xor_shift_128_plus algorithm
def xs128p(state0, state1, browser):
    s1 = state0 & MASK
    s0 = state1 & MASK
    s1 ^= (s1 << 23) & MASK
    s1 ^= (s1 >> 17) & MASK
    s1 ^= s0 & MASK
    s1 ^= (s0 >> 26) & MASK 
    state0 = state1 & MASK
    state1 = s1 & MASK
    if browser == 'chrome':
        generated = state0 & MASK
    else:
        generated = (state0 + state1) & MASK

    return state0, state1, generated

# Symbolic execution of xs128p
def sym_xs128p(slvr, sym_state0, sym_state1, generated, browser):
    s1 = sym_state0 
    s0 = sym_state1 
    s1 ^= (s1 << 23)
    s1 ^= LShR(s1, 17)
    s1 ^= s0
    s1 ^= LShR(s0, 26) 
    sym_state0 = sym_state1
    sym_state1 = s1
    if browser == 'chrome':
        calc = sym_state0
    else:
        calc = (sym_state0 + sym_state1)
    
    condition = Bool('c%d' % int(generated * random.random()))
    if browser == 'chrome':
        impl = Implies(condition, LShR(calc, 12) == int(generated))
    elif browser == 'firefox' or browser == 'safari':
        # Firefox and Safari save an extra bit
        impl = Implies(condition, (calc & 0x1FFFFFFFFFFFFF) == int(generated))

    slvr.add(impl)
    return sym_state0, sym_state1, [condition]

def sym_xs128p_lossy(slvr, sym_state0, sym_state1, generated_lower, generated_higher, browser):
    s1 = sym_state0 
    s0 = sym_state1 
    s1 ^= (s1 << 23)
    s1 ^= LShR(s1, 17)
    s1 ^= s0
    s1 ^= LShR(s0, 26) 
    sym_state0 = sym_state1
    sym_state1 = s1
    if browser == 'chrome':
        calc = sym_state0
    else:
        calc = (sym_state0 + sym_state1)
    
    condition = Bool('c%d' % int(generated_lower * random.random()))
    if browser == 'chrome':
        impl1 = Implies(condition, LShR(calc, 12) >= int(generated_lower))
        impl2 = Implies(condition, LShR(calc, 12) <= int(generated_higher))
        both = And(impl1, impl2)
    # we dont care about these
    # elif browser == 'firefox' or browser == 'safari':
    #     # Firefox and Safari save an extra bit
    #     impl = Implies(condition, (calc & 0x1FFFFFFFFFFFFF) == int(generated))

    slvr.add(both)
    return sym_state0, sym_state1, [condition]

def reverse17(val):
    return val ^ (val >> 17) ^ (val >> 34) ^ (val >> 51)

def reverse23(val):
    return (val ^ (val << 23) ^ (val << 46)) & MASK

def xs128p_backward(state0, state1, browser):
    prev_state1 = state0
    prev_state0 = state1 ^ (state0 >> 26)
    prev_state0 = prev_state0 ^ state0
    prev_state0 = reverse17(prev_state0)
    prev_state0 = reverse23(prev_state0)
    # this is only called from an if chrome
    # but let's be safe in case someone copies it out
    if browser == 'chrome':
        generated = prev_state0
    else:
        generated = (prev_state0 + prev_state1) & MASK
    return prev_state0, prev_state1, generated

def to_double(browser, out):
    if browser == 'chrome':
        double_bits = (out >> 12) | 0x3FF0000000000000
        double = struct.unpack('d', struct.pack('<Q', double_bits))[0] - 1
    elif browser == 'firefox':
        double = float(out & 0x1FFFFFFFFFFFFF) / (0x1 << 53) 
    elif browser == 'safari':
        double = float(out & 0x1FFFFFFFFFFFFF) * (1.0 / (0x1 << 53))
    return double

def get_states(nums, browser, lossy=False):
    # assume, maybe, that the last dub we have is constrained...
    if browser == 'chrome':
        dubs = nums[::-1]
    # from the doubles, generate known piece of the original uint64 
    generated = []
    for idx in range(len(dubs)):
        if browser == 'chrome':
            if isinstance(dubs[idx], tuple):
                lower_bound = struct.unpack('<Q', struct.pack('d', dubs[idx][0] + 1))[0] & (MASK >> 12)
                upper_bound = struct.unpack('<Q', struct.pack('d', dubs[idx][1] + 1))[0] & (MASK >> 12)
                recovered = (lower_bound, upper_bound)
            else:
                recovered = struct.unpack('<Q', struct.pack('d', dubs[idx] + 1))[0] & (MASK >> 12)
        elif browser == 'firefox':
            recovered = dubs[idx] * (0x1 << 53) 
        elif browser == 'safari':
            recovered = dubs[idx] / (1.0 / (1 << 53))
        generated.append(recovered)

    # setup symbolic state for xorshift128+
    ostate0, ostate1 = BitVecs('ostate0 ostate1', 64)
    sym_state0 = ostate0
    sym_state1 = ostate1
    slvr = Solver()
    conditions = []

    # run symbolic xorshift128+ algorithm for three iterations
    # using the recovered numbers as constraints
    for ea in range(len(dubs)):
        if isinstance(generated[ea], tuple):
            sym_state0, sym_state1, ret_conditions = sym_xs128p_lossy(slvr, sym_state0, sym_state1, generated[ea][0], generated[ea][1], browser)
        else:
            sym_state0, sym_state1, ret_conditions = sym_xs128p(slvr, sym_state0, sym_state1, generated[ea], browser)
        conditions += ret_conditions

    if slvr.check(conditions) == sat:
        # get a solved state
        m = slvr.model()
        state0 = m[ostate0].as_long()
        state1 = m[ostate1].as_long()
        slvr.add(Or(ostate0 != m[ostate0], ostate1 != m[ostate1]))
        if slvr.check(conditions) == sat:
            print('WARNING: multiple solutions found! use more dubs!')
            return "MULTIPLE"
        else:
            return (state0, state1)
    else:
        return "UNSAT"
        
def get_forward(state0, state1):
    state0, state1, out = xs128p(state0, state1, 'chrome')
    double = to_double('chrome', out)
    return state0, state1, double

def get_backward(state0, state1):
    state0, state1, out = xs128p_backward(state0, state1, 'chrome')
    double = to_double('chrome', out)
    return state0, state1, double

class MockXorshift128PlusV8Cache():
    def __init__(self, state0, state1):
        self.state0 = state0
        self.state1 = state1
        self.cache = []
        for _ in range(64):
            # gets g1, g2, so forth...
            self.state0, self.state1, next = get_forward(self.state0, self.state1)
            self.cache.append(next)
            
    def pop(self):
        if len(self.cache) == 0:
            for _ in range(64):
                self.state0, self.state1, next = get_forward(self.state0, self.state1)
                self.cache.append(next)
        return self.cache.pop()
    

import json
import os

os.chdir(os.path.dirname(os.path.abspath(__file__)))

START_OFFSET = 20
SIZE = 8

def reverse(nums, target, max=676000):
    cur_candidate = nums[START_OFFSET:START_OFFSET + SIZE]
    offset = START_OFFSET
    cur_candidate = [ (float_get_lower_upper_bound(n)) for n in cur_candidate]
    states = get_states(cur_candidate, 'chrome', True)
    if states == "UNSAT":
        return "UNSAT"
    elif states == "MULTIPLE":
        print("need more dubs")
        return "MULTIPLE"
    else:
        while True:
            state0, state1 = states
            state0, state1, n = get_forward(state0, state1)
            state0, state1, n = get_backward(state0, state1)
            new_candidate = cur_candidate[1:] + [ float_get_lower_upper_bound(nums[offset + SIZE]) ]
            offset += 1
            states = get_states(new_candidate, 'chrome', lossy=True)
            if states == "UNSAT":
                break
            elif states == "MULTIPLE":
                print("something went wrong during solve")
            else:
                cur_candidate = new_candidate
    mock_cache = MockXorshift128PlusV8Cache(state0, state1)
    start_of_mock_idx = offset - 57 # cos size
    for i in range(start_of_mock_idx, 605 * 50):
        n = mock_cache.pop()
        if math.floor(n * max) == target:
            return i
    return "RETRY"