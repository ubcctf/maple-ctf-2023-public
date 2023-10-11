import random
import base64

random.seed(12350981234987213598234)
num_nodes = 1000

# 1. Generate a Connected Graph
graph = {i: {} for i in range(num_nodes)}
unvisited = set(range(num_nodes))
current_node = random.choice(list(unvisited))
unvisited.remove(current_node)

# Use a set to keep track of used weights to ensure uniqueness
used_weights = set()
all_edges = set()

MAX = 100_000
while unvisited:
    next_node = random.choice(list(unvisited))
    
    # Ensure the weight is unique
    weight = random.randint(1, MAX)
    while weight in used_weights:
        weight = random.randint(1, MAX)
    used_weights.add(weight)
    
    graph[current_node][next_node] = weight
    graph[next_node][current_node] = weight
    all_edges.add((min(current_node, next_node), max(current_node, next_node), weight))
    unvisited.remove(next_node)
    current_node = next_node

for _ in range(num_nodes * 2):
    u, v = random.sample(range(num_nodes), 2)
    if v not in graph[u]:
        weight = random.randint(1, 100000)
        while weight in used_weights:
            weight = random.randint(1, 100000)
        used_weights.add(weight)
        graph[u][v] = weight
        graph[v][u] = weight
        all_edges.add((min(u, v), max(u, v), weight))


# 2. Find the MST using Prim's Algorithm
mst_edges = set()
visited = {random.choice(list(graph.keys()))}
while len(visited) < num_nodes:
    crossing = [(u, v, graph[u][v]) for u in visited for v in graph[u] if v not in visited]
    u, v, weight = min(crossing, key=lambda x: x[2])
    mst_edges.add((min(u, v), max(u, v), weight))
    
    visited.add(v)

edges_str = ';'.join(f'{u},{v},{weight}' for u, v, weight in all_edges)

with open("sum_of_weights.txt", "w") as f:
    f.write(str(sum([k[2] for k in mst_edges])))
    
with open("graph.txt", "w") as f:
    f.write(base64.b64encode(edges_str.encode('utf-8')).decode())

mst_edges = sorted(list(mst_edges))

key = ''.join([str(edge[0]) + str(edge[1]) for edge in mst_edges])
answer = ','.join([str(edge[0]) + ","+str(edge[1]) for edge in mst_edges])
with open("answer.txt", "w") as f:
    f.write(answer)


from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import padding
import hashlib

def encrypt(key, plaintext):
    digest = hashlib.sha256()
    digest.update(key.encode('utf-8'))
    hashed_key = digest.digest()[:16]

    cipher = Cipher(algorithms.AES(hashed_key), modes.ECB())
    padder = padding.PKCS7(128).padder()
    padded_plaintext = padder.update(plaintext.encode('utf-8')) + padder.finalize()

    encryptor = cipher.encryptor()
    encrypted_data = encryptor.update(padded_plaintext) + encryptor.finalize()

    byte_array_str = ", ".join(f"(byte) {b}" for b in encrypted_data)
    print(f"byte[] encrypted = new byte[]{{{byte_array_str}}};")

encrypt(key, "maple{y0u_f0und_th3_mst!}")
