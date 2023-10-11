FROM python:3.8-alpine

WORKDIR /app
COPY ./src /app
RUN pip3 install -r requirements.txt
RUN unzip images.zip

CMD ["flask", "run", "--host=0.0.0.0"]
