from PIL import Image
from flask import Flask, request, render_template
from base64 import b64encode
import os
import glob

app = Flask(__name__)


@app.route("/")
def home():
    files = list(filter(os.path.isfile, glob.glob("images/*")))
    # files.sort(key=os.path.getmtime)
    files.sort()
    with open(files[0], "rb") as image:
        return render_template("index.html", text="Welcome to madness. Scan 777 codes for a flag. Here is your first picture:", image=str(b64encode(image.read()))[2:-1])

@app.route("/submit")
def submit():
    arg = request.args.get("value")
    files = list(filter(os.path.isfile, glob.glob("images/*")))
    # files.sort(key=os.path.getmtime)
    files.sort()
    for i in range(len(files)):
        file = files[i]
        if arg == file[7:-4]:
            if i == len(files)-1:
                with open("flag.txt", "r") as flag:
                    with open(files[i], "rb") as image:
                        return render_template("index.html", text="Congratulations! You've earned this flag: " + flag.readlines()[0], image=str(b64encode(image.read()))[2:-1])
            else:
                with open(files[i+1], "rb") as image:
                    return render_template("index.html", text="Correct! You have " + str(len(files) - i - 1) + " pictures to go. Here is your next picture:", image=str(b64encode(image.read()))[2:-1])
    with open(files[0], "rb") as image:
        return render_template("index.html", text="Sorry, that's not a valid code.", image=str(b64encode(image.read()))[2:-1])
