FROM python:3-slim
RUN apt-get update -y && apt-get dist-upgrade -y && apt-get install socat -y && pip install pycryptodome
COPY ./src /app

CMD ["socat", "TCP-LISTEN:1337,reuseaddr,fork", "EXEC:\"python /app/server.py\""]