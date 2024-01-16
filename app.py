# -------------------------------------------------------------------------------------------------------- #
# IOT, WaterBnB Project - Flask server                                                                     #
# -------------------------------------------------------------------------------------------------------- #
# 16 Jan 2024, Université Côte d'Azur,                                                                     #
# CRafael Baptista.                                                                                        #
# -------------------------------------------------------------------------------------------------------- #

from flask import Flask, request
app = Flask(__name__)

@app.route('/')
def hello_world():
    return 'Welcome to pool P_22301479, By Rafael!'

# Print get parameters from the request /open?idu=Rafael&idswp=P_22301479
@app.route('/open')
def open():
    idu   = request.args.get('idu', 'No idu provided')
    idswp = request.args.get('idswp', 'No idswp provided')
    return f'idu: {idu}, idswp: {idswp}'
