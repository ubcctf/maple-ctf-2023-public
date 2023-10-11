FROM ubuntu:22.04
RUN apt-get update -y && apt-get dist-upgrade -y && apt-get install xinetd -y

RUN useradd -M ctf
WORKDIR /ctf
COPY ./dist/lost-in-space ./
COPY ./hosted/flag.txt ./
COPY ./hosted/xinetd.conf ./
COPY ./hosted/run.sh ./

CMD ["xinetd", "-d", "-dontfork", "-f", "xinetd.conf"]
