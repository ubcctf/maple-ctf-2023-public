from typing import List
from sage.modules.free_module_integer import IntegerLattice
import itertools


N = 5
SZ = 128
BLIND = 96


# Borrowed from rbtree https://hackmd.io/@hakatashi/B1OM7HFVI
def babai(mat, target):
    M = IntegerLattice(mat, lll_reduce=True).reduced_basis
    G = M.gram_schmidt()[0]
    diff = target
    for i in reversed(range(G.nrows())):
        diff -= M[i] * ((diff * G[i]) / (G[i] * G[i])).round()
    return target - diff


def recover_state(outputs: List[int], positions: List[int], a: int) -> int:
    M = matrix(ZZ, N, N)
    outputs = vector(ZZ, outputs)

    for i, pos in zip(range(N), positions):
        M[i, i] = 2 ^ SZ
        M[0, i] = a ^ pos

    res = babai(M, outputs)
    if all(
        [(guess >> BLIND) << BLIND == output for guess, output in zip(res, outputs)]
    ):
        return res
    return False


def produce_next_outputs(state, a):
    outputs = []
    for _ in range(42):
        outputs.append((state >> BLIND) << BLIND)
        state = (state * a) % (2 ^ SZ)
    return outputs


a1 = 17858755236422136913
a2 = 10444850750214055793
flag = [3999539808, 1592738381, 1057217965, 215730455, 2499659667]
outputs = [3110779950, 3143489116, 2523808356, 59145943, 424415688, 1607693531, 2579126212, 1755297842, 3906113295, 1470215707, 3409703846, 3241626049, 3619900521, 3320623221, 2749059114, 775644902, 2452534658, 1107040405, 1783853908, 280554339, 3216758786, 2250874382, 2218107153, 4254508193, 2241158217, 2648593639, 2984582005, 3238054409, 3573713662, 2295623647, 1012063687, 1503914767, 2705122053, 2969541370, 2233703326, 1334624347, 1016155206, 2288145534, 2614694809, 1778390279, 999900406, 2501497460]


def check_state(state, a, values):
    """
    Return true if the original state matches the truncated values produced:
    """
    for v in values:
        if ((state >> BLIND) << BLIND) != v:
            return False
        state = (state * a) % (2 ^ SZ)
    return True


outputs = [(num << BLIND) for num in outputs]

for indices in itertools.combinations(range(2 * N), N):
    diffs_ranges = [list(range(indices[i + 1] - indices[i] + 2)) for i in range(N - 1)]

    for diffs in itertools.product(*diffs_ranges):
        powers = [0] + list(itertools.accumulate(diffs))
        for a in a1, a2:
            state = recover_state([outputs[i] for i in indices], powers, a)
            if not state:
                continue

            # Check that you have the correct state by simulating the remaining outputs
            # And checking that at least xx match correctly
            cnt = 0
            forward_states = produce_next_outputs(state[-1], a)
            for idx in range(2 * N, 42):
                if outputs[idx] in forward_states:
                    cnt += 1

            if cnt >= 5:
                print("FOUND", "RNG 1" if a == a1 else "RNG 2", state, indices)

"""
FOUND RNG 1 (249052866576226715931086667681322899277, 4686024463024066771954312834456351853, 33625675122822813975739215516615386509, 204339430720102433843906712381109962141, 139069022727690120350514139753234638013) (1, 3, 4, 6, 7)
FOUND RNG 1 (249052866576226715931086667681322899277, 4686024463024066771954312834456351853, 33625675122822813975739215516615386509, 204339430720102433843906712381109962141, 116482489010910177757915298624261817293) (1, 3, 4, 6, 9)
FOUND RNG 1 (249052866576226715931086667681322899277, 4686024463024066771954312834456351853, 33625675122822813975739215516615386509, 139069022727690120350514139753234638013, 116482489010910177757915298624261817293) (1, 3, 4, 7, 9)
FOUND RNG 1 (249052866576226715931086667681322899277, 4686024463024066771954312834456351853, 204339430720102433843906712381109962141, 139069022727690120350514139753234638013, 116482489010910177757915298624261817293) (1, 3, 6, 7, 9)
FOUND RNG 1 (249052866576226715931086667681322899277, 33625675122822813975739215516615386509, 204339430720102433843906712381109962141, 139069022727690120350514139753234638013, 116482489010910177757915298624261817293) (1, 4, 6, 7, 9)
FOUND RNG 1 (4686024463024066771954312834456351853, 33625675122822813975739215516615386509, 204339430720102433843906712381109962141, 139069022727690120350514139753234638013, 116482489010910177757915298624261817293) (3, 4, 6, 7, 9)
"""

for last_idx in range(10, 42):
    indices = [0, 2, 5, 8, last_idx]
    diffs_ranges = [list(range(indices[i + 1] - indices[i] + 2)) for i in range(N - 1)]
    values = [outputs[i] for i in indices]

    for diffs in itertools.product(*diffs_ranges):
        powers = [0] + list(itertools.accumulate(diffs))
        state = recover_state(values, powers, a2)
        if not state:
            continue

        cnt = 0
        forward_states = produce_next_outputs(state[-1], a2)
        for idx in range(10, 42):
            if outputs[idx] in forward_states:
                cnt += 1

        if cnt >= 5:
            print("FOUND", state, indices)

"""
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 217802902287942066610900644165206845977) [0, 2, 5, 8, 14]
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 61452920403641110046063832872491279369) [0, 2, 5, 8, 15]
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 194309814489644368855222430412650840809) [0, 2, 5, 8, 16]
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 337076866589942191571341964153000977017) [0, 2, 5, 8, 23]
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 283138766833787689261509157322023942473) [0, 2, 5, 8, 28]
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 181878043423858849559098037233853720633) [0, 2, 5, 8, 29]
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 80183946347631575380311639577107509289) [0, 2, 5, 8, 30]
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 235271306272067959356174425302457418009) [0, 2, 5, 8, 33]
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 176972210181798777249251504691370589689) [0, 2, 5, 8, 34]
FOUND (246461379432507198548744869519312616425, 199956698639826841165021921495410454745, 127374604373723580577393435234258838953, 309474178971347778413815283363423185785, 105739834733895532479811560252306098665) [0, 2, 5, 8, 35]
"""

seed1 = 249052866576226715931086667681322899277
seed2 = 246461379432507198548744869519312616425

pt = b""

# note: since rng 1 starts at indice 1, there may or may not have been an extra shift at the start
seed1 = seed1 * pow(a1, -1, 2 ^ SZ) % (2 ^ SZ)

for i in range(len(flag) - 1, -1, -1):
    seed1 = seed1 * pow(a1, -1, 2 ^ SZ) % (2 ^ SZ)
    seed2 = seed2 * pow(a2, -1, 2 ^ SZ) % (2 ^ SZ)
    pt = int(flag[i] ^^ int(seed1 >> BLIND) ^^ int(seed2 >> BLIND)).to_bytes(4, 'big') + pt

print(pt)