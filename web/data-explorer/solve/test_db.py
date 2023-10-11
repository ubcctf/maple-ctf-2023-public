import sqlite3
import os
from leaker import *

cols, data = get_table()

conn = sqlite3.connect(":memory:")
conn.execute("CREATE TABLE license (key)")
key = os.urandom(128).hex()
conn.execute("INSERT INTO license VALUES (?)", (key,))
conn.execute("CREATE TABLE data (%s)" % ", ".join(cols))
for row in data:
    conn.execute("INSERT INTO data VALUES (%s)" % ",".join(["?"] * len(row)), row)

def query(col, od):
    curs = conn.execute('SELECT * FROM data ORDER BY %s %s' % (col, od))
    return curs.fetchall()

print(key)
result = leak("SELECT key FROM license", query) + leak("SELECT substr(key, 129) FROM license", query)
print(result)
assert key == result
