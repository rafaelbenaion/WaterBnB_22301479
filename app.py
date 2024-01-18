# -------------------------------------------------------------------------------------------------------- #
# IOT, WaterBnB Project - Flask server                                                                     #
# -------------------------------------------------------------------------------------------------------- #
# 16 Jan 2024, Université Côte d'Azur,                                                                     #
# Rafael Baptista.                                                                                         #
# -------------------------------------------------------------------------------------------------------- #

import json
import csv

from flask import request
from flask import jsonify
from flask import Flask
from flask import session
from flask import render_template
from flask_mqtt import Mqtt
from flask_pymongo import PyMongo
from pymongo import MongoClient
from datetime import datetime

# https://python-adv-web-apps.readthedocs.io/en/latest/flask.html
# https://www.emqx.com/en/blog/how-to-use-mqtt-in-flask

# -------------------------------------------------------------------------------------------------------- #
# Mongo database connection                                                                                #
# -------------------------------------------------------------------------------------------------------- #

piscines = {} # list of swimming pools active
ADMIN    = False
client   = MongoClient("mongodb+srv://admin:admin@waterbnb.lo1mkvx.mongodb.net/")

# db is an attribute of client =>  all databases

# -------------------------------------------------------------------------------------------------------- #
# Looking for "WaterBnB" database                                                                          #
# -------------------------------------------------------------------------------------------------------- #

dbname  = 'WaterBnB'
dbnames = client.list_database_names()
if dbname in dbnames:
    print(f"{dbname} is there!")
else:
    print("YOU HAVE to CREATE the db !\n")

db = client.WaterBnB

# -------------------------------------------------------------------------------------------------------- #
#  Looking for "users" collection in the WaterBnB database                                                 #
# -------------------------------------------------------------------------------------------------------- #

collname  = 'users'
collnames = db.list_collection_names()

if collname in collnames:
    print(f"{collname} is there!")
else:
    print(f"YOU HAVE to CREATE the {collname} collection !\n")

userscollection = db.users                                                                # collection users
piscinesCol     = db.piscines                                                          # collection piscines

# -------------------------------------------------------------------------------------------------------- #
#  Import authorized users                                                                                 #
# -------------------------------------------------------------------------------------------------------- #

if ADMIN:
    userscollection.delete_many({})  # empty collection
    excel = csv.reader(open("usersM1_2023.csv"))  # list of authorized users
    for l in excel:
        ls = (l[0].split(';'))
        if userscollection.find_one({"name": ls[0]}) == None:
            userscollection.insert_one({"name": ls[0], "num": ls[1]})

# -------------------------------------------------------------------------------------------------------- #
#  Initialisation :  Flask service                                                                         #
# -------------------------------------------------------------------------------------------------------- #

app = Flask(__name__)

# Notion de session ! .. to share between routes !
# https://flask-session.readthedocs.io/en/latest/quickstart.html
# https://testdriven.io/blog/flask-sessions/
# https://www.fullstackpython.com/flask-globals-session-examples.html
# https://stackoverflow.com/questions/49664010/using-variables-across-flask-routes
app.secret_key = 'BAD_SECRET_KEY'

# -------------------------------------------------------------------------------------------------------- #
#  Route /                                                                                                 #
# -------------------------------------------------------------------------------------------------------- #

@app.route('/')
def hello_world():
    return render_template('index.html')  # 'Hello, World!'

# -------------------------------------------------------------------------------------------------------- #
#  Route /post                                                                                             #
# -------------------------------------------------------------------------------------------------------- #

#https://stackabuse.com/how-to-get-users-ip-address-using-flask/
@app.route("/ask_for_access", methods=["POST"])
def get_my_ip():
    ip_addr = request.remote_addr
    return jsonify({'ip asking ': ip_addr}), 200

# Test/Compare with  =>curl  https://httpbin.org/ip

#Proxies can make this a little tricky, make sure to check out ProxyFix
#(Flask docs) if you are using one.
#Take a look at request.environ in your particular environment :
@app.route("/ask_for_access", methods=["POST"])
def client():
    ip_addr = request.environ['REMOTE_ADDR']
    return '<h1> Your IP address is:' + ip_addr

# -------------------------------------------------------------------------------------------------------- #
#  Route /open                                                                                             #
# -------------------------------------------------------------------------------------------------------- #

@app.route("/open", methods=['GET', 'POST'])
# @app.route('/open') # ou en GET seulement

def openthedoor():

    # ---------------------------------------------------------------------------------------------------- #
    #  Verify if the request can be accepted                                                               #
    # ---------------------------------------------------------------------------------------------------- #

    idu              = request.args.get('idu')                               # idu : clientid of the service
    idswp            = request.args.get('idswp')                           # idswp : id of the swimming pool
    session['idu']   = idu
    session['idswp'] = idswp

    print("\n Peer = {}".format(idu))

    piscinesCol = db.piscines                                          # Recover piscines data from database
    piscineInfo = piscinesCol.find_one({"_id": idswp})["data"][-1]    # Recover last data info from the pool

    ip_addr     = request.environ.get('HTTP_X_FORWARDED_FOR', request.remote_addr)    # ip addresses of user

    if userscollection.find_one({"name": idu}) is not None and piscineInfo is not None:

        if piscineInfo["occuped"] is False:                             # Check if the swimming pool is free
            granted = "YES"
        else:
            granted = "NO"
    else:
        granted = "NO"

    return jsonify({'idu': session['idu'], 'idswp': session['idswp'], "granted": granted}), 200

# Test with => curl -X POST https://waterbnbf.onrender.com/open?who=gillou
# Test with => curl https://waterbnbf.onrender.com/open?who=gillou

# -------------------------------------------------------------------------------------------------------- #
#  Route /users                                                                                            #
# -------------------------------------------------------------------------------------------------------- #

@app.route("/users")
def lists_users():  # Liste des utilisateurs déclarés
    todos = userscollection.find()
    return jsonify([todo['name'] for todo in todos])

# -------------------------------------------------------------------------------------------------------- #
#  Route /publish                                                                                          #
# -------------------------------------------------------------------------------------------------------- #

@app.route('/publish', methods=['POST'])
def publish_message():
    request_data = request.get_json()
    publish_result = mqtt_client.publish(request_data['topic'], request_data['msg'])
    return jsonify({'code': publish_result[0]})


# -------------------------------------------------------------------------------------------------------- #
# MQTT                                                                                                     #
# -------------------------------------------------------------------------------------------------------- #

app.config['MQTT_BROKER_URL'] = "test.mosquitto.org"
app.config['MQTT_BROKER_PORT'] = 1883
# app.config['MQTT_USERNAME'] = ''  # Set this item when you need to verify username and password
# app.config['MQTT_PASSWORD'] = ''  # Set this item when you need to verify username and password
# app.config['MQTT_KEEPALIVE'] = 5  # Set KeepAlive time in seconds
app.config['MQTT_TLS_ENABLED'] = False  # If your broker supports TLS, set it True

topicname = "uca/iot/piscine"
mqtt_client = Mqtt(app)

@mqtt_client.on_connect()
def handle_connect(client, userdata, flags, rc):
    if rc == 0:
        print('Connected successfully')
        mqtt_client.subscribe(topicname)  # subscribe topic
    else:
        print('Bad connection. Code:', rc)


@mqtt_client.on_message()
def handle_mqtt_message(client, userdata, msg):
    global topicname
    global piscines
     # list of swimming pools active

    data = dict(
        topic=msg.topic,
        payload=msg.payload.decode()
    )
    #    print('Received message on topic: {topic} with payload: {payload}'.format(**data))
    print("\n msg.topic = {}".format(msg.topic))
    print("\n topicname = {}".format(topicname))

    # ---------------------------------------------------------------------------------------------------- #
    # Writing piscines data from MQTT in the database                                                      #
    # ---------------------------------------------------------------------------------------------------- #

    if (msg.topic == topicname):

        decoded_message = str(msg.payload.decode("utf-8"))
        #print("\ndecoded message received = {}".format(decoded_message))

        dic = json.loads(decoded_message)                                              # From string to dict
        #print("\n Dictionnary  received = {}".format(dic))

        ident    = dic["info"]["ident"]                                                      # Qui a publié ?
        temp     = dic["status"]["temperature"]                                        # Quelle température ?
        occuped  = dic["piscine"]["occuped"]                                                     # Occupied ?
        date     = datetime.now().strftime("%d/%m/%Y %H:%M:%S")                                      # When ?

        new_data = {'temp': temp, "occuped": occuped, "published": date}                 # Data to be stored
        insertData(ident, new_data)                                  # Insert data in the database

        piscines[ident] = new_data
        print("\n piscines = {}".format(piscines))
        with open("output.txt", "w") as file:
            json.dump(piscines, file, indent=4)

# -------------------------------------------------------------------------------------------------------- #
# Function for data insertion in the database                                                              #
# -------------------------------------------------------------------------------------------------------- #
def insertData(piscine_id, data):

    if piscinesCol.count_documents({"_id": piscine_id}) == 0:           # Check if piscine is already stored
        piscinesCol.insert_one({                                              # Insert new piscine with data
            "_id": piscine_id,
            "data": [data],
        })
    else:
        piscinesCol.update_one(                                       # Piscine exists so just push new data
            {"_id": piscine_id},
            {"$push": {"data": data}}
        )

# -------------------------------------------------------------------------------------------------------- #
# Main                                                                                                     #
# -------------------------------------------------------------------------------------------------------- #

if __name__ == '__main__':
    # run() method of Flask class runs the application
    # on the local development server.
    app.run(debug=False)  # host='127.0.0.1', port=5000)

