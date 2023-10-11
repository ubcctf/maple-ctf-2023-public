FROM ubuntu:22.04
RUN apt-get update -y && apt-get dist-upgrade -y && apt-get install socat -y

RUN useradd -M ctf
WORKDIR /ctf

COPY ./dist/src/chal ./

CMD ["socat", "TCP-LISTEN:1337,reuseaddr,fork", "EXEC:\"./chal\""]