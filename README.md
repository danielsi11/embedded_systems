# embedded_systems

Coursework 1

Three folders are created for the separate uses:

1. Raspberry Pi
This folder contains all the necessary files required in the Raspberry Pi to obtain data from the accelerometer and temperature sensor. The data will also be published to a MQTT server to be received by a client.

2. Subscriber
This folder contains all the necessary files for a user to subscribe to a topic on the MQTT server in order to receive the message sent from the Raspberry Pi. A JSON file is also created to be used for the user interface demonstration.

3. Demo
This folder contains all the necessary files to display the user interface. Node.js and Socket.io are used to create a real-time web connection between the server and the local webpage. Data from the JSON file is processed before displaying in a table and a graph.


Coursework 2

File contains the code used for the Mbed microcontroller.
