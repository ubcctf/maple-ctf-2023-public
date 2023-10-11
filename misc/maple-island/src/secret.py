flag = b"Love will never give up on you. Love will never let you down. Love will never run around and desert you maple{G05h_1_w4nt_4_st4bl3_m4tch_t00_pls_1_4m_50_l0n3ly}"


def create_a_perfect_world(ones, zeroes, oprefs, zprefs):
    one_matches = [False] * len(ones)
    zero_matches = [False] * len(zeroes)
    total = len(zeroes)
    couples_left_to_make = total

    def z_prefers_o_over_o1(z, o, o1):
        return zprefs[z].index(ones[o]) < zprefs[z].index(ones[o1])

    while couples_left_to_make > 0:
        o = 0
        while o < total:
            if one_matches[o] == False:
                break
            o += 1

        i = 0
        while i < total and one_matches[o] == False:
            z = zeroes.index(oprefs[o][i])
            if zero_matches[z] == False:
                zero_matches[z] = o
                one_matches[o] = z
                couples_left_to_make -= 1

            else:
                if z_prefers_o_over_o1(z, o, zero_matches[z]):
                    o1 = zero_matches[z]
                    zero_matches[z] = o
                    one_matches[o] = z
                    one_matches[o1] = False
            i += 1

    couples = []
    for o, match in enumerate(one_matches):
        couples.append(ones[o] + zeroes[match])

    return couples
