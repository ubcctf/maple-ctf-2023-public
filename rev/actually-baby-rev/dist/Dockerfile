FROM ubuntu:22.04
RUN apt-get update -y && apt-get dist-upgrade -y && apt-get install socat -y
COPY ./src /app

CMD ["socat", "TCP-LISTEN:1337,reuseaddr,fork", "EXEC:\"/app/chal\""]