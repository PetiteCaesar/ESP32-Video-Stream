import cv2
from PIL import ImageGrab, Image, ImageEnhance
from mss import mss
import numpy as np
import time
import sys
import websocket
import time

print("starting")

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
ip = ""
i = 0
s = 0
quality = 40
ws = websocket.WebSocket()
ws.connect("ws://"+ip)
print("Connected to WebSocket server")
dis = 0
rec = True
aspect = 16
noCompQ = quality
compA = False 
comp = True

uf = 10

compThresh = 6000 #normal 3400
ft = 0



ws.send("dis"+str(dis))
if rec:
    ws.send("recv") #reciveing
else:
    ws.send("notrecv")

while True:
    try:
        try:


            stn = time.time()
            if aspect == 16:
                #img = ImageGrab.grab(bbox=(0, 0, 1920, 1080)) #16:9
                img = capture_Screen()
                #img = capture_Region(400,600,240,240)
                #img = ImageGrab.grab(bbox=None,include_layered_windows=False,all_screens=False,xdisplay=None)
            elif aspect == 4:
                img = capture_Region(240, 0, 1680, 1080) #4:3
            elif aspect == 5:
                img = capture_Region(240, 0, 1440, 1080) #idk 💀
            elif aspect == 6:
                img = capture_Region(285, 0, 1350, 1080) #idk 💀
            else:
                img = capture_Region(54, 0, 800, 640) #16:9

            capt = time.time()-stn



            img_np = np.array(img)

            frame = cv2.cvtColor(img_np, cv2.COLOR_BGR2RGB)
            #frame = cv2.cvtColor(img_np, cv2.COLOR_BGR2G   RAY)
            if dis == 0:
                if aspect == 16:
                    #final = cv2.resize(frame,(120,64))
                    #final = cv2.resize(frame,(160,90)) #16:9
                    #final = cv2.resize(frame,(240,128)) #16:9
                    #final = cv2.resize(frame,(480,270))
                    #final = cv2.resize(frame,(354,200))
                    final = cv2.resize(frame,(302,170))
                else:
                    #final = cv2.resize(frame,(160,128))
                    final = cv2.resize(frame,(240,240))
            else:
                final = cv2.resize(frame,(128,64))
            

            jpeg = cv2.imencode('.jpg', final,[int(cv2.IMWRITE_JPEG_QUALITY), quality])[1].tobytes()

            if compA:
                if comp:
                    if sys.getsizeof(jpeg) > compThresh:
                        #print("Frame size: ", sys.getsizeof(jpeg))
                        q = sys.getsizeof(jpeg) - compThresh
                        d = quality-int(q / 100)
                        
                        if d >= quality:
                            d = 10
                        #print(d)
                        jpeg = cv2.imencode('.jpg', final,[int(cv2.IMWRITE_JPEG_QUALITY), d])[1].tobytes()
                        if i % uf==0:
                            #print("Frame size after: ", sys.getsizeof(jpeg),d)
                            pass
                else:
                    s = sys.getsizeof(jpeg)
                    if s > compThresh:
                        d = int((noCompQ-(s/1000))) if int((noCompQ-(s/1000))) < quality else quality
                        print(d)
                        jpeg = cv2.imencode('.jpg', final,[int(cv2.IMWRITE_JPEG_QUALITY), d])[1].tobytes()
            else:
                if sys.getsizeof(jpeg) > 12000:
                    jpeg = cv2.imencode('.jpg', final,[int(cv2.IMWRITE_JPEG_QUALITY), 10])[1].tobytes()

            ws.send_binary(jpeg)
            
            
            ft = round(time.time()-stn,4)

            if i % uf==0:
                    print(ft,"BFPS:",round((1/(time.time()-stn)),2))
                    print("Frame size: ", sys.getsizeof(jpeg))
            if not rec:
                time.sleep(0.008)
                pass
            else:
                #result = ws.recv()
                result = ws.recv_frame()
                #print(result)
                # if i % 5==0:
                #     print("Recv FPS:",time.time()-cap)


            # if capt > 30:
            #     ttn = time.time()
            #     while True:
            #         current_time = time.time()
            #         elapsed_time = current_time - ttn
            #         if elapsed_time > ((1/30)-ttn+stn):
            #             break

            fps = round(1/(time.time()-stn),2)
            if i % uf==0:
                print("FPS:",fps,"\n")
                i = 0
            i+=1
        except KeyboardInterrupt:
            print("KeyboardInterrupt, Attempting to close (please work)")
            ws.close()
            break
    except:
        print("Error: Disconnected from Server")
        print("Attempting to Reconnect...")
        ws.connect("ws://"+ip)#esp32 ip address
        print("Connected to Server")
        

ws.close()