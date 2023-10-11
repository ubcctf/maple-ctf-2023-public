FROM ubuntu:22.04

RUN apt-get update && apt-get install -y python3 python3-bottle xz-utils && apt-get clean
RUN useradd -d /home/ctf -m ctf

WORKDIR /home/ctf

COPY ./dist/db.tar.xz /home/ctf/
RUN tar --strip-components=1 -xvf db.tar.xz && chown ctf:ctf film.db && rm db.tar.xz
COPY ./hosted/flag.txt /home/ctf/flag.txt

USER ctf
EXPOSE 8080/tcp
CMD python3 web.py
