import cv2
from PIL import ImageGrab, Image, ImageEnhance
from mss import mss
import numpy as np
import time
import sys
import websocket
import time

print("Starting")

#monitor = {"top": 0, "left": 0, "width": 1920, "height": 1080}
monitor = {"top": 200, "left": 100, "width": 1280, "height": 720}

def capture_Screen():
    # Capture entire screen
        with mss() as sct:
            return cv2.cvtColor(np.array(sct.grab(monitor)), cv2.COLOR_BGR2RGB)
            
def capture_Region(x,y,w,h):
    # Capture entire screen
    with mss() as sct:
        monitor = {"top": y, "left": x, "width": w, "height": h}
        sct_img = sct.grab(monitor)
        # Convert to PIL/Pillow Image
        return Image.frombytes('RGB', sct_img.size, sct_img.bgra, 'raw', 'BGRX')


#local ip of esp32 
ip = "192.168.0.143"
quality = 80
rec = True
verbosity = 10
i = 0
ws = websocket.WebSocket()
ws.connect("ws://"+ip)
print("Connected to WebSocket server")

if rec:
    ws.send("recv") #Should send back ack
else:
    ws.send("notrecv")

while True:
    try:
        try:


            stn = time.time()
            img = capture_Screen()

            frame = cv2.cvtColor(np.array(img), cv2.COLOR_BGR2RGB)
            final = cv2.resize(frame,(240,135))
            jpeg = cv2.imencode('.jpg', final, [int(cv2.IMWRITE_JPEG_QUALITY), quality])[1].tobytes()

            ws.send_binary(jpeg)
                    
            if rec:
                result = ws.recv_frame()
                
            fps = round(1/(time.time()-stn),2)
            if i % verbosity==0:
                print("\nFPS:",fps)
                print("Frame size: ", sys.getsizeof(jpeg))
                i = 0
            i+=1
            
        except KeyboardInterrupt:
            print("KeyboardInterrupt, Attempting to close")
            ws.close()
            break
    except:
        print("Error: Disconnected from Server")
        print("Attempting to Reconnect...")
        ws.connect("ws://"+ip)#esp32 ip address
        print("Connected to Server")
        

ws.close()