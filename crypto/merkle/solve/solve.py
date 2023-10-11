from pwn import *
import hashlib
import string
import random

r = remote("localhost", 1337)

def sign(msg):
    r.sendlineafter(b"> ", b'1')
    r.sendlineafter(b"> ", msg)

    half_sig_path = r.recvline().strip().split(b",")
    priv = r.recvline().strip().split(b",")

    half_sig_path = [bytes.fromhex(sig.decode()) for sig in half_sig_path]
    priv = [bytes.fromhex(key.decode()) for key in priv]

    return half_sig_path, priv

def verify(msg, sig_path, msg_sig, pub):
    r.sendlineafter(b"> ", b'2')
    r.sendlineafter(b"> ", msg)
    r.sendlineafter(b"> ", str(sig_path).encode())
    r.sendlineafter(b"> ", str(msg_sig).encode())
    r.sendlineafter(b"> ", str(pub).encode())

    return r.recvline().decode().strip()

def get_root_hash():
    r.sendlineafter(b"> ", b'3')
    return bytes.fromhex(r.recvline().decode().strip())

def rand_msg():
    return ''.join([random.choice(string.ascii_letters) for _ in range(8)]).encode()

def hash(m):
    return hashlib.sha256(m).digest()

def bytes_to_bin(m):
    k = bin(int.from_bytes(hash(m), 'big'))[2:].zfill(256)
    return list(map(int, k))



h = 6
root_hash = get_root_hash()
all_hashes = set([root_hash])

cnt = 0
results = []

# 1. query enough times to get all unique signatures in the tree
while len(all_hashes) < 2 ** (h + 1) - 2:
    msg = rand_msg()
    half_sig_path, priv = sign(msg)
    all_hashes.update(half_sig_path)
    results.append((half_sig_path, priv, msg))
    cnt += 1

print(f"Took {cnt} queries to determine tree")


# 2. Reconstruct the parent and sibling relationships between hashes (nodes)
parent = {hash: () for hash in all_hashes}
sibling = {hash: () for hash in all_hashes}

for h1 in all_hashes:
    for h2 in all_hashes:
        p = hash(h1 + h2)
        if p in all_hashes:
            sibling[h1] = (h2, True)
            sibling[h2] = (h1, False)

            parent[h1] = parent[h2] = p

print(f"Reconstructed parent and siblings")

# 3. Keep querying until we have determined the full private key for any leaf node to forge b'flag'
recovered_priv_keys = []
for half_sig_path, priv, msg in results:
    # If a private key with the same bits already exists in our recovered list, then update any other bits for that key
    # Otherwise, add this new private key to the recovered list
    key = [[None for _ in range(256)] for i in range(2)]
    found = False
    msg_bin = bytes_to_bin(msg)
    for recovered_priv_key in recovered_priv_keys:
        if all([recovered_priv_key[c][i] is None or priv[i] == recovered_priv_key[c][i] for i, c in enumerate(msg_bin)]):
            key = recovered_priv_key
            found = True

    for i, c in enumerate(msg_bin):
        key[c][i] = priv[i]

    if not found:
        recovered_priv_keys.append(key)

# return a list of (hash, 1), (hash, 0) representing the siblings along the path
def recover_signature_path(hash):
    signature_path = []
    assert hash in all_hashes
    while hash != root_hash:
        signature_path.append(sibling[hash])
        hash = parent[hash]
    return signature_path

flag_bin = bytes_to_bin(b'flag')
while True:
    msg = rand_msg()
    half_sig_path, priv = sign(msg)
    msg_bin = bytes_to_bin(msg)

    for recovered_priv_key in recovered_priv_keys:
        # Update any missing bits in the private key that signed the current message
        if all([recovered_priv_key[c][i] is None or priv[i] == recovered_priv_key[c][i] for i, c in enumerate(msg_bin)]):
            for i, c in enumerate(msg_bin):
                recovered_priv_key[c][i] = priv[i]

            # Once you fully recover a private key, you win
            if all([recovered_priv_key[c][i] is not None for i in range(256) for c in range(2)]):
                print("Found a full private key")
                pub = [[None for _ in range(256)] for i in range(2)]
                msg_sig = []
                for i, c in enumerate(flag_bin):
                    pub[c][i] = hash(recovered_priv_key[c][i])
                    msg_sig.append(recovered_priv_key[c][i])

                pub = [[hash(recovered_priv_key[i][j]) for j in range(256)] for i in range(2)]
                leaf_sig = hash(b''.join(pub[0] + pub[1]))
                signature_path = recover_signature_path(leaf_sig)
                print(verify(b'flag', signature_path, msg_sig, pub))
                exit()