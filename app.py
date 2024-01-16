# -------------------------------------------------------------------------------------------------------- #
# IOT, WaterBnB Project - Flask server                                                                     #
# -------------------------------------------------------------------------------------------------------- #
# 16 Jan 2024, Université Côte d'Azur,                                                                     #
# CRafael Baptista.                                                                                        #
# -------------------------------------------------------------------------------------------------------- #

from urllib import request
from flask import Flask
app = Flask(__name__)

@app.route('/')
def hello_world():
    #return 'Hello, Rafa!'
    idu  = request.args.get('idu', 'No idu provided')
    idswp = request.args.get('idswp', 'No idswp provided')
    return f'idu: {idu}, idswp: {idswp}'

# Print all get parameters from the request /open?idu=Rafael&idswp=P_22301479
#@app.route('/open')
#def open():
