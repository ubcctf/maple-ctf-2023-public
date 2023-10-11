FROM python:3.11-slim
RUN apt-get update -y && apt-get dist-upgrade -y && apt-get install socat -y
COPY ./src /app

CMD ["socat", "TCP-LISTEN:1337,reuseaddr,fork", "EXEC:\"python -u /app/pow.py python -u /app/server.py\""]