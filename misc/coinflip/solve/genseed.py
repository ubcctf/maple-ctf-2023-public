from MersenneTwister import PythonMT19937
import z3

s = z3.Solver()
keys = [z3.BitVec("key%d" % i, 32) for i in range(624)]
mt = PythonMT19937()
mt.init_by_array(keys)

for i in range(1, 624):
    s.add(mt.mt[i] == 0)

res = s.check()
m = s.model()
key_vals = [m[k].as_long() for k in keys]

seed = 0
for i in range(len(key_vals)):
    seed |= key_vals[i] << (32 * i)

print(hex(seed))
