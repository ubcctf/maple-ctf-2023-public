from Crypto.Cipher import DES
from Crypto.Util.Padding import pad
from multiprocessing import Pool
import hashlib


def bxor(a, b):
    return bytes([i ^ j for i, j in zip(a, b)])


def expand_key(key, n):
    if len(key) >= n:
        return key[:n]

    out = key + b"\x00" * (n - len(key))
    for i in range(1, n - len(key) + 1):
        out = bxor(out, b"\x00" * i + key + b"\x00" * (n - len(key) - i))
    return out


traffic = [('Client', 'Hello'), ('Server', b'J\xfb\xace\xcc\xe6k\x87\x16B<\x03\xc8\xa2Ef'), ('Client', (b'\x0b\xd4\x06d\xfa\x1f\x15\xc7\xf2\xe8\\+\xa3\xf3V\x1fv\x9a\x99`\x8bR\n\xe0 \x87\xc7\xf0d\x91\xc7\xc65|\xa2n\x8f2\x92taK\x193\xed\xb3\xa3Y', b'\xe0_\xc9:\r\xbaF$', 'admin')), ('Server', b"\xcaL\x86\xe60u\x1c'\xf9\x16\xae\x95i\x83\x1c\x14\xb4\x8e\xef\xa5")]
server_challenge = traffic[1][1]
challenge_response, challenge_hash, username = traffic[2][1]
padded_chall_hash = pad(challenge_hash, 16)
target = [challenge_response[:16], challenge_response[16:32], challenge_response[32:]]


def check_key(key):
    key = expand_key(key.to_bytes(3, "little"), 8)
    enc = DES.new(key, DES.MODE_ECB).encrypt(padded_chall_hash)

    for i in range(3):
        if enc == target[i]:
            print(f"keys[{i}] = {key}")


def generate_challenge_response(password, challenge_hash):
    password_hash = hashlib.md5(password.encode()).digest()
    print(expand_key(password_hash[13:16], 8))
    challenge_response = DES.new(
        expand_key(password_hash[0:3], 8), DES.MODE_ECB
    ).encrypt(pad(challenge_hash, 16))
    challenge_response += DES.new(
        expand_key(password_hash[7:10], 8), DES.MODE_ECB
    ).encrypt(pad(challenge_hash, 16))
    challenge_response += DES.new(
        expand_key(password_hash[13:16], 8), DES.MODE_ECB
    ).encrypt(pad(challenge_hash, 16))
    return challenge_response


if __name__ == "__main__":
    # Use this to generate keys:
    # with Pool(50) as p:
    #     p.map(check_key, range(2 ** 24))

    keys = [0, 0, 0]
    keys[0] = b"\xb5\xecNNNN\xfb\xa2"
    keys[1] = b"\xaa\xd8LLLL\xe6\x94"
    keys[2] = b"\xb8\xc6))))\x91\xef"

    response = b""
    challenge_hash = b"a" * 16
    for i in range(3):
        response += DES.new(keys[i], DES.MODE_ECB).encrypt(pad(challenge_hash, 16))

    print(response.hex(), challenge_hash.hex(), "admin")
