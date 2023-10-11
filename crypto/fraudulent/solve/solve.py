# https://eprint.iacr.org/2016/771.pdf page 9

from Crypto.Util.number import getPrime, long_to_bytes, bytes_to_long
from random import randint
from hashlib import sha256

def hash(values):
    h = sha256()
    for v in values:
        h.update(long_to_bytes(v))
    return bytes_to_long(h.digest())

def verify_vote(encrypted_vote, proof):
    R, S = encrypted_vote
    c_0, c_1, f_0, f_1 = proof

    values = [
        pow(g, f_0, p) * pow(R, -c_0, p) % p,
        pow(X, f_0, p) * pow(S, -c_0, p) % p,
        pow(g, f_1, p) * pow(R, -c_1, p) % p,
        pow(X, f_1, p) * pow(S, -c_1, p) * pow(g, c_1, p) % p,
    ]

    return c_0 + c_1 == hash(values)


p = 81561774084914804116542793383590610809004606518687125749737444881352531178029
q = p - 1
g = 2
m = -98
a_0, b_0, a_1, b_1 = 1, 1, 1, 1

assert pow(37, p-1, p) == 1

# Generate params
A_0 = pow(g, a_0, p)
B_0 = pow(g, b_0, p)
A_1 = pow(g, a_1, p)
B_1 = pow(g, b_1, p)

c = hash([A_0, B_0, A_1, B_1])
x = ((b_0 + c * m) * (1 - m) - b_1 * m) * pow(a_0 * (1 - m) - a_1 * m, -1, q) % q
X = pow(g, x, p)

# Create encryption of m
r = randint(0, q - 1)
R, S = pow(g, r, p), pow(g, m, p) * pow(X, r, p) % p

c_1 = (b_1 - a_1 * x) * pow(1 - m, -1, q) % q
c_0 = c - c_1

f_0 = a_0 + c_0 * r
f_1 = a_1 + c_1 * r


print("Private key:", x)
print("(R, S):", (R, S))
print("(c_0, c_1, f_0, f_1):", (c_0, c_1, f_0, f_1))

print(verify_vote((R, S), (c_0, c_1, f_0, f_1)))

