import socket
import thread
import time
import sys

result = ""

def nmeaGps():
    global result
    host = "192.168.43.1" #Device IP
    port = 50000
    PACKET_SIZE=1152 # how many characters to read at a time
    sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    sock.connect((host,port)) #connect to the device
    while True: #continuously read and handle data
        data = sock.recv(PACKET_SIZE)
        try:
            for line in data.split("\r\n"):
                if line.startswith('$GPGGA'):
                    lat, NS, lon, EW = line.strip().split(',')[2:6]
                    Decilat = int(float(lat)/100) + (float(lat)-int(float(lat)/100)*100)/60
                    Decilon = int(float(lon)/100) + (float(lon)-int(float(lon)/100)*100)/60
                    if (NS=='S'):
                        Decilat = (-1)*Decilat
                    if (EW=='W'):
                        Decilon = (-1)*Decilon
                    result = str(Decilat) + "," + str(Decilon)
        except:
            pass
            
thread.start_new_thread(nmeaGps,())

while True:
    time.sleep(10)
    sys.stdout.write(result+"\n")
    sys.stdout.flush()
