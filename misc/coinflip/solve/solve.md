## Solution

Python allows you to seed an RNG with an arbitrarily large number, which is split into 32-bit chunks and fed to a seeding routine. We can invert the seeding routine to find a seed that will make almost every bit of the MT state equal to zero (the seeding process forces `mt[0] = 0x80000000`, so we cannot control that one bit).

Once the MT state is nearly zero, almost every output it produces will be equal to zero, and the RNG will not return to a proper random distribution for millions of bits, allowing us to easily win the coin flip game.

`genseed.py` generates an appropriate seed, which is shown in `input.txt`. `cat input.txt - | nc ...` is enough to get a flag.
