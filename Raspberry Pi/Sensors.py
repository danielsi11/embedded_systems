import smbus
import time
import json
import math

bus = smbus.SMBus(1)

#For accelerometer initialisation
bus.write_byte_data(0x18,0x20,0x27) #enabling normal power mode - 10Hz
acc_x = 0.0
acc_y = 0.0
acc_z = 0.0

#For temperature sensor initialisation
bus.write_i2c_block_data(0x40, 0x02, [0x70,0x00])

import paho.mqtt.client as mqtt
client = mqtt.Client()
client.tls_set(ca_certs="mosquitto.org.crt",certfile="client.crt",keyfile="client.key") #files needed for encryption
client.connect("test.mosquitto.org",port=8884)

try:
    while True:
        #temperature sensor
        tempin = bus.read_i2c_block_data(0x40, 0x01, 2)
        temp = (tempin[0] << 8) |  tempin[1]    #combine two bytes of data into a single 16-bit number
        if temp > 32767:	#convert unsigned number to signed number if most significant bit is 1
            temp -= 65536

        temp = round(temp/128,3) #rounding to 3 decimal places

        #accelerometer sensor
        LSB = bus.read_byte_data(0x18, 0x28)    #read least significant byte from FIFO for x-axis
        MSB = bus.read_byte_data(0x18, 0x29)    #read most significant byte from FIFO for x-axis

        acc_x = (MSB << 8) | LSB    #combine the two bytes into a single 16-bit number

        if acc_x > 32767:   #convert unsigned number to signed number if most significant bit is 1
            acc_x -= 65536

        acc_x = acc_x / 16384   #normalise number to the range 2G (can be changed)

        #For y-axis
        LSB = bus.read_byte_data(0x18, 0x2A)
        MSB = bus.read_byte_data(0x18, 0x2B)

        acc_y = (MSB << 8) | LSB
        if acc_y > 32767:
            acc_y -= 65536

        acc_y = acc_y / 16384

        #For z-axis
        LSB = bus.read_byte_data(0x18, 0x2C)
        MSB = bus.read_byte_data(0x18, 0x2D)

        acc_z = (MSB << 8) | LSB
        if acc_z > 32767:
            acc_z -= 65536
			acc_z = acc_z / 16384

        #special case when dividing by 0
        if(acc_z == 0){
            roll = 90
            roll = round(roll, 3)   #rounding to 3 decimal places
        }
        else {
            roll = math.fabs(math.atan(acc_y/acc_z) * (180/math.pi))    #roll equation
            roll = round(roll, 3)
        }

        pitch = math.fabs(math.atan(acc_x/ math.sqrt((acc_y*acc_y)+(acc_z*acc_z))) * (180/math.pi)) #pitch equation
        pitch = round(pitch, 3)
        time.sleep(1)   #delay for reading data continuously in loop

		#store the sensor data in a dict type
        acc_data = dict(
        temp_data = temp,
        pitch_data = pitch,
        roll_data = roll,
        )

        payload = json.dumps(acc_data)  #JSON encoding
        client.publish("IC.embedded/TuttiFruitti/IoT",payload)  #publish to the MQTT server with a topic

except KeyboardInterrupt:
    print('interrupted')
