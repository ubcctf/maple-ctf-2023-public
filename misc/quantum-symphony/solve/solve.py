#!/usr/bin/env python3

from qiskit import QuantumCircuit
from qiskit.compiler import transpile
from qiskit.circuit.library import StatePreparation
import numpy as np
from math import sqrt
from pwn import *

QUBITS = 4

# 12 Bar Blues Progression found from the sheet music
CHORD_PROGRESSION = ['Cmaj', 'Cmaj', 'Cmaj', 'Cmaj', 
                     'Fmaj', 'Fmaj', 'Cmaj', 'Cmaj',
                     'Gmaj', 'Fmaj', 'Cmaj', 'Gmaj'] 

# Since a chord in the CMaj scale is 3 notes, the ideal statevector needed 
# is where there is an equal superpositon (or likelihood) of exactly three states/notes
# using fractions helps avoid precision issues because the probabilities must add to 1.

# A 4 qubit state vector has 16 entries. Thus, there is a probability for each of the 16 states 
# the qubit register is in.
CHORD_TO_PROBABILITIES_MAP = {
    'Cmaj': [1/3, 0, 0, 0, 1/3, 0, 0, 1/3, 0, 0, 0, 0, 0, 0, 0, 0],
    'Fmaj': [1/3, 0, 0, 0, 0, 1/3, 0, 0, 0, 1/3, 0, 0, 0, 0, 0, 0],
    'Gmaj': [0, 0, 1/3, 0, 0, 0, 0, 1/3, 0, 0, 0, 1/3, 0, 0, 0, 0]
}

# A helper function that turns probabilities to wave amplitudes
def probabilities_to_statevector(probabilities: list[int]):
    state = np.array([sqrt(e) for e in probabilities])
    return state


p = process(['/usr/bin/python3', 'server.py'])

for i, chord in enumerate(CHORD_PROGRESSION):
    pause(1)
    info(p.clean().decode())
    qc = QuantumCircuit(QUBITS)
    sv = probabilities_to_statevector(CHORD_TO_PROBABILITIES_MAP[chord])
    # the State preparation method enables you to create a series of gates that will
    # convert a ground state into the specified state vector with fancy math.
    qc.append(StatePreparation(sv), range(QUBITS))
    # Use transpile to avoid parsing issues with the qasm to qiskit conversion process
    transpiled_circ = transpile(qc, basis_gates=['rx', 'ry', 'rz', 'cx'])
    qasm_str = transpiled_circ.qasm()
    info(f"chord number {i}, a {chord}")
    debug(qasm_str)
    p.sendline(b64e(qasm_str.encode()).encode())
p.interactive()
