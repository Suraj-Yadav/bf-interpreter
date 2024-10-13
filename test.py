#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test
"""

from random import randint

import sympy as sp

x, y, z, w = sp.symbols("x y z w")


def fibo(w, x, y, z):
    while w != 0:
        (x, y) = (y, x + y)
        w = w - 1
    return w, x, y, z


def multiply(w, x, y, z):
    while w != 0:
        y += x
        z += x
        x = y
        y = 0
        w = w - 1
    return w, x, y, z


# what we want to solve for
def function(w, x, y, z):
    return fibo(w, x, y, z)


v = (1 + x + x * x) * (1 + y + y * y) * (1 + z + z * z) * (1 + w + w * w)
# Limit v to terms which are at max degree of 2
v = v.expand().as_terms()
v = [1, w, w * x, w * y, x, y, z]

print(v)

# Generate coeff for each term
coeffs = [sp.symbols(f"c{i}") for i in range(len(v))]

# combine term and coeff into single expression exp
# This is the general form function can take
# We now need to determine values of coeffs
exp = 0
for a, b in zip(coeffs, v):
    exp = exp + a * b
# Generate some points for generating equations
points = [
    (1, 2, 4, 7),
    (2, 5, 5, 4),
    (5, 6, 1, 5),
    (3, 3, 6, 5),
    (6, 1, 6, 6),
    (6, 4, 4, 1),
    (7, 7, 4, 6),
]
# for i in range(len(coeffs)):
#     points.append(
#         (
#             randint(1, 10),
#             randint(1, 10),
#             randint(1, 10),
#             randint(1, 10),
#         )
#     )


# points.append((1, 1, 1, 0))

# points1 = [e for e in points]
# for e in points:
#     points1.append((e[0], e[1], e[2], 0))
#
# points = points1

syms = [w, x, y, z]

for i, s in enumerate(syms):
    # for each symbol we create equations using the points and solve for coeffs
    eqs = []
    bs = []
    for j, p in enumerate(points):
        eq = exp.subs(dict(zip(syms, p)))
        print(eq)
        # print(f"a[{j}] = {{", end=" ")
        # for k in range(len(coeffs)):
        # v = [0] * len(coeffs)
        # v[k] = 1
        # print(eq.subs(dict(zip(coeffs, v))), end=",")
        # print(v)
        # print("};")
        b = function(*p)[i]
        bs.append(b)
        eqs.append(eq - b)
    # print(*bs, sep=",")
    sol = sp.solve(eqs, coeffs)
    # assigning coeffs back into exp gives the final value of symbol after function run
    print(s, "after function =", exp.subs(sol))


print(exp)
# print(function(1, 1, 1, 0))
