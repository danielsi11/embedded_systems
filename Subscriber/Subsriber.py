import time
import paho.mqtt.client as mqtt
import json

client = mqtt.Client()
#client.tls_set(ca_certs="mosquitto.org.crt",certfile="client.crt",keyfile="client.key")
#client.connect("test.mosquitto.org", port=8884) #Encrypted port
#client.connect("test.mosquitto.org", port=1883)    #Unencrypted port
client.connect("broker.hivemq.com", port=1883)

def on_message(client, userdata, message) :
    data = json.loads((message.payload).decode("utf-8"))    #message received is of type byte, converts it into a string before JSON decoding

    with open('C:\\Users\\ALICIASI\\Desktop\\TuttiFruitti_CW1\\Demo\\data.json', 'w') as outfile:   #change directory for the output accordingly
        json.dump(data, outfile) #create a json file and store the received message from MQTT server inside

client.on_message = on_message
client.loop_start()
client.subscribe("IC.embedded/TuttiFruitti/IoT")    #subscribe to the topic on the MQTT server

while (True):  #create an infinite loop to prevent python from exiting
    time.sleep(1)
