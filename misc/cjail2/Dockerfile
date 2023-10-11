FROM python:3-slim
RUN apt-get update -y && apt-get dist-upgrade -y && apt-get install socat gcc -y
COPY ./src/flag /app/flag
COPY ./dist/jail.py /app/jail.py

CMD ["socat", "TCP-LISTEN:1337,reuseaddr,fork", "EXEC:\"python /app/jail.py\",stderr"]
