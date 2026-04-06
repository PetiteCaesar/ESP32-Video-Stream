#include <Arduino.h>
#include <JPEGDEC.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include ".config.h"
#include <TFT_eSPI.h>


#define THREADED


uint8_t content[1] = {0};
bool rec = false;

WebSocketsServer webSocket = WebSocketsServer(80);

TFT_eSPI tft = TFT_eSPI();

TaskHandle_t drawTask;
TaskHandle_t networkTask;

bool kickedOff = false;

JPEGDEC jpeg;

struct FrameBuffer{

    FrameBuffer()=default;
    ~FrameBuffer() {
        if(m_canFree) delete[] m_frameBuf;
    }
    bool CreateBuf(uint32_t w, uint32_t h){
        m_width = w; m_height = h;
        m_size = w * h;
        m_frameBuf = new uint16_t[m_size];
        FillBuffer(0);
        if(!m_frameBuf) return false;
        m_canFree = true;
        return true;
    }

    bool CreateBuf(uint8_t* pool, uint32_t w, uint32_t h, bool handleLifetime = false){
        m_width = w; m_height = h;
        m_size = w * h;
        m_frameBuf = (uint16_t*)pool;
        FillBuffer(0);
        if(!m_frameBuf) return false;
        m_canFree = handleLifetime;
        return true;
    }

    void FillBuffer(uint16_t val){
        if(!m_frameBuf) return;
        memset(m_frameBuf,val,m_size<<1);
    }
    void WriteSection(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t* data){
        if(!m_frameBuf || !data || x >= m_width || y >= m_height) return;
        
        //w + x <= m_width -> w <= x + m_width
        uint32_t rowCopyLength = min(w, m_width-x);
        //y + h <= m_height -> h <= m_height - y
        uint32_t rowsToCopy = min(h,m_height - y);

        uint16_t* start = m_frameBuf + y*m_width + x;
        while(rowsToCopy--){
            memcpy(start, data, rowCopyLength<<1);
            start += m_width;
            data += w;
        }
    }

    void CopyBuffer(const uint8_t* buf, uint32_t size){
        memcpy(m_frameBuf, buf, size);
    }

    inline uint32_t GetSizeBytes() const {return (m_width * m_height) << 1;}

    inline const uint16_t* GetBuf()const {return m_frameBuf;}

    private:
        uint32_t m_width, m_height;
        uint32_t m_size;
        uint16_t* m_frameBuf = nullptr;
        bool m_canFree = false;
};

FrameBuffer decode;
FrameBuffer draw1;
FrameBuffer draw2;

int JPEGDraw(JPEGDRAW *pDraw) {
    decode.WriteSection(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
    return 1;
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {

    switch (type) {

        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
        break;

        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connection from ", num);
                Serial.println(ip.toString());
            }
        break;

        case WStype_TEXT:
            Serial.printf("[%u] Text: %s\n", num, payload);
            webSocket.sendTXT(num, payload);
            payload[length] = 0;
            if (strcmp((char *)payload, "recv") == 0) {
                rec = true;
            }
            else if (strcmp((char *)payload, "notrecv") == 0) {
                rec = false;
            }

        break;

        case WStype_BIN: {
            // Serial.println("Got bin");
            if(jpeg.openRAM((uint8_t *)payload, length, JPEGDraw)){
                auto t = millis();
                if(jpeg.decode(0,0,0)){
                    // Serial.printf("Decode took: %" PRIu32 "\n",millis()-t);
                    #ifdef THREADED
                    if(kickedOff) ulTaskNotifyTake(pdTRUE, portMAX_DELAY); //make sure drawing finished
                    #endif
                    //wait for drawing to complete
                    draw1.CopyBuffer((uint8_t*)decode.GetBuf(), draw1.GetSizeBytes());
                    draw2.CopyBuffer((uint8_t*)decode.GetBuf()+draw1.GetSizeBytes(), draw2.GetSizeBytes());
                    // Serial.println("Copied to draw 1 and 2, give noti");
                    //tell draw task it can start drawing
                    #ifdef THREADED
                    xTaskNotifyGive(drawTask);
                    #else
                    t = millis();
                    int draw1Height = (int)TFT_WIDTH/2;
                    tft.pushImage(0,0,TFT_HEIGHT,draw1Height,draw1.GetBuf());
                    tft.pushImage(0,draw1Height,TFT_HEIGHT,TFT_WIDTH-draw1Height,draw2.GetBuf());
                    Serial.printf("memcpy + draw: %" PRIu32 "\n",millis()-t);
                    #endif
                }
                jpeg.close();
            }
            webSocket.sendBIN(num, content, 1);
            //wait to be told it can run
            // Serial.println("Wait for noti (draw)");
            
            // Serial.println("got noti");
        }
        break;
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        default:
        break;
    }
}

void drawLoop(void* pvParameters) {
    uint32_t lastDraw = 0;
    for(;;) {
        //wait to be asked
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        auto s = millis();
        int draw1Height = (int)TFT_WIDTH/2;
        tft.pushImage(0,0,TFT_HEIGHT,draw1Height,draw1.GetBuf());
        tft.pushImage(0,draw1Height,TFT_HEIGHT,TFT_WIDTH-draw1Height,draw2.GetBuf());
        auto dt = millis()-s;
        
        // Serial.printf("Draw buf time: %" PRIu32 " \n",dt); 
        Serial.printf("Time since lastDraw complete: %" PRIu32 "\n", millis() - lastDraw);
        lastDraw = millis();
        xTaskNotifyGive(networkTask);//tell its done
        kickedOff = true;//for initial "kick off"
    }
}

void networkLoop(void* params){
    for(;;){
        auto s = millis();
        webSocket.loop();
        auto dt = millis()-s;
        // Serial.printf("webLoop: %" PRIu32 " \n",dt); 
    }
}



void setup() {
    Serial.begin(115200);
    delay(1000);

    auto avail1 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    Serial.print("Avail1 DRAM: ");
    Serial.println(avail1);
    if(!decode.CreateBuf(TFT_HEIGHT,TFT_WIDTH)){
        Serial.println("Failed to create decode buffer");
    }
    auto avail2 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    Serial.print("Avail2 DRAM: ");
    Serial.println(avail2);
    int draw1Height = (int)TFT_WIDTH/2;
    if(!draw1.CreateBuf(TFT_HEIGHT,draw1Height)){
        Serial.println("Failed to create draw1 buf");
    }
    auto avail3 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    Serial.print("Avail2 DRAM: ");
    Serial.println(avail3);
    if(!draw2.CreateBuf(TFT_HEIGHT,TFT_WIDTH-draw1Height)){
        Serial.println("Failed to create draw2 buf");
    }
    


    tft.init();

    tft.setRotation(3);
    tft.setTextColor(0xFFFF, 0x0000);
    tft.fillScreen(TFT_RED);
    tft.setSwapBytes(true);



    Serial.print("Connecting to network: ");
    Serial.println(SSID);

    WiFi.begin(SSID, PASSWORD);
    uint8_t counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        counter++;
        if (counter >= 60) { // after 30 seconds timeout - reset board
            esp_restart();
        }
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address set: ");
    Serial.println(WiFi.localIP()); // print LAN IP

    tft.fillScreen(TFT_BLACK);

    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);

    #ifdef THREADED
    xTaskCreatePinnedToCore(drawLoop, "DrawTask", 2048, nullptr, 16, &drawTask, 0);
    xTaskCreatePinnedToCore(networkLoop, "NetworkTask", 8192, nullptr, 16, &networkTask, 1);
    #endif

  
}
void loop(){
    #ifdef THREADED
    vTaskDelay(portMAX_DELAY);
    #else
    webSocket.loop();
    #endif
}