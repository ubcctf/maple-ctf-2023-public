from pwn import *

file = open("input.txt", "rb")
seed = next(file).strip()
rounds = next(file).strip()

s = remote(args.HOST, args.PORT)
s.sendlineafter(b"Which coin would you like to use? ", seed)
s.sendlineafter(b"How many rounds do you want to go for? ", rounds)
s.interactive()
