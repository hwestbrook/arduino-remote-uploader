#include <SoftwareSerial.h>

// TODO
// figure out why it fails completely when debug is disabled. probably timing issue
// parse channel on new connection, close connection

#define ESP_RX   3
#define ESP_TX   4
SoftwareSerial espSerial(ESP_RX, ESP_TX);

#define readLen 6
// fails without debug enabled, what??
#define DEBUG
#define BUFFER_SIZE 128
#define LISTEN_PORT "1111"

// replace with wifi creds
#define WIFI_NETWORK ""
#define WIFI_PASSWORD ""

// may not be necessary. instead do a AT command and only reset if no response
#define RESET_MINS 180
//#define SEND_AT_EVERY_MINS 1

long resetEvery = RESET_MINS * 60000;
long lastReset = 0;

char cbuf[BUFFER_SIZE];

// TODO keep track of which channels are open/closed

Stream* debug;
Stream* esp;

// FIXME handle array of connections
//bool[] connections = new bool[5];
int lastConnection = -1;

void setup() {
  // first run AT to see if alive
  delay(3000);
  
  espSerial.begin(9600); // Soft serial connection to ESP8266
  esp = &espSerial;
  
  #ifdef DEBUG
    Serial.begin(9600); while(!Serial); // UART serial debug
    debug = &Serial;
  #endif

  //configureEsp8266();
  configureServer();
  
  lastReset = millis();
}

//int print(char* text) {
//  int len = strlen(text);
//  
//  #ifdef DEBUG
//    debug->print("-->");
//    // ugh
//    esp->print(len);
//    debug->print(text);
//  #endif
//
//  int writeLen = esp->print(text);
//  
//  if (writeLen != len) {
//    #ifdef DEBUG
//      debug->print("Write fail. wrote x, but exp. y");
//    #endif    
//  }
//  
//  return writeLen;
//}

void resetEsp8266() {
  if (lastConnection != -1) {
    closeConnection(lastConnection);
  }
  
  stopServer();
  sendReset();
  configureServer();
    
  resetCbuf(cbuf, BUFFER_SIZE);
  lastReset = millis();  
}

int printDebug(char* text) {
  #ifdef DEBUG
    return debug->print(text);
  #endif
  
  return -1;
}

// from adafruit lib
void debugLoop(void) {
  if(!debug) for(;;); // If no debug connection, nothing to do.

  debug->println("\n========================");
  for(;;) {
    if(debug->available())  esp->write(debug->read());
    if(esp->available()) debug->write(esp->read());
  }
}

void configureEsp8266() {
/*
<---
AT(13)(13)(10)(13)(10)OK(13)(10)<--[11c],[14ms],[1w]
<---
AT+RST(13)(13)(10)(13)(10)OK(13)(10).|(209)N;(255)P:L(179)(10)BD;G8\(21)E5(205)N^f(5)(161)O'}5"B(201)(168)HHWV.V(175)H(248)<--[58c],[1926ms],[4w]
<---
AT+CWMODE=3(13)(13)(10)(13)(10)OK(13)(10)<--[20c],[25ms],[1w]
<---
AT+CIFSR(13)(13)(10)+CIFSR:APIP,"192.168.4.1"(13)(10)+CIFSR:APMAC,"1a:fe:34:9b:a7:4c"(13)(10)+CIRTP0.0(10)CRSM,8e4ba4(13)(10)(13)<--[96c],[164ms],[1w]
<---
AT+CIPMUX=1(13)(13)(10)(13)(10)OK(13)(10)<--[20c],[25ms],[1w]
<---
AT+CPSRV(164)(245)(197)b(138)(138)(138)(138)(254)(13)(13)(10)(13)(10)OK(13)(10)<--[26c],[18ms],[0w]
<---
AT+CIPMUX=1(13)(13)(10)(13)(10)OK(13)(10)<--[20c],[24ms],[1w]
<---
AT+C(160)M(21)IY(21)I(138)b(138)(138)(254)1(13)(13)(10)no(32)change(13)(10)<--[31c],[21ms],[0w]
*/

  sendAt();
  sendReset();
  sendCwmode();
  //joinNetwork();
  sendCifsr();
  configureServer();
  enableMultiConnections();
  startServer();
    
  resetCbuf(cbuf, BUFFER_SIZE);
  lastReset = millis();
}

// config to apply after reset or power cycle. everthing else should be retained
void configureServer() {
  enableMultiConnections();
  startServer();
}

// TODO static ip: AT+CIPSTA

int sendAt() {
  esp->print("AT\r\n");
  
  readFor(100);  
  
  if (strstr(cbuf, "OK") == NULL) {
      return 0;
  }
  
  return 1;
}  

int sendReset() {
  esp->print("AT+RST\r\n");
  
  readFor(2500);  
  
  if (strstr(cbuf, "OK") == NULL) {
      #ifdef DEBUG
        printDebug("RST fail");        
      #endif
      
      return 0;
  }
  
  return 1;
}  

int sendCwmode() {
  esp->print("AT+CWMODE=3\r\n");
  
  readFor(100);
  
  if (strstr(cbuf, "OK") == NULL) {
      #ifdef DEBUG
        printDebug("fail");        
      #endif    
      
      return 0;
  }
  
  return 1;
}

int joinNetwork() {  
  esp->print("AT+CWJAP=\""); 
  esp->print(WIFI_NETWORK); 
  esp->print("\",\"");
  esp->print(WIFI_PASSWORD);
  esp->print("\"\r\n");
  
  readFor(10000);
  
  if (strstr(cbuf, "OK") == NULL) {
      #ifdef DEBUG
        Serial.println("Join fail");        
      #endif    
      
      return 0;
  }
  
  return 1;
}

int sendCifsr() {
  // on start we could publish our ip address to a known entity
  // or, get from router, or use static ip, 
  esp->print("AT+CIFSR\r\n");
  readFor(200);
  
  // TODO parse ip address
  if (strstr(cbuf, "AT+CIFSR") == NULL) {
      #ifdef DEBUG
        Serial.println("CIFSR fail");        
      #endif    
      
      return 0;
  }
  
  return 1;
}

// required for server mode
// set to 0 for single connection
int enableMultiConnections() {
  esp->print("AT+CIPMUX=1\r\n"); 
  
  // if still connected: AT+CIPMUX=1(13)(13)(10)link(32)is(32)builded(13)(10)
  readFor(200);    

  if (!(strstr(cbuf, "OK") != NULL || strstr(cbuf, "builded"))) {
      #ifdef DEBUG
        Serial.println("CIPMUX fail");        
      #endif    
      
      return 0;
  }
  
  return 1;
}

int closeConnection(int id) {
  esp->print("AT+CIPCLOSE=");
  esp->print(id);
  esp->print("\r\n"); 
  
  readFor(200);    

  if (strstr(cbuf, "OK") == NULL) {
      #ifdef DEBUG
        Serial.println("Close conn fail");        
      #endif    
      
      return 0;
  }
  
  return 1;  
}

// must enable multi connections prior to calling
int startServer() {
  startStopServer(true);  
}

// stop server and close connections. must call reset after
int stopServer() {
  startStopServer(false);
}

int startStopServer(bool start) {
  esp->print("AT+CIPSERVER="); 
  if (start) {
    esp->print(1);
  } else {
    esp->print(0);
  }
  
  if (start) {
    esp->print(",");
    esp->print(LISTEN_PORT);    
  }

  esp->print("\r\n");  
  
  readFor(500);  
  
  if (!(strstr(cbuf, "OK") != NULL || strstr(cbuf, "no change") || strstr(cbuf, "restart"))) {
      #ifdef DEBUG
        Serial.println("CIPSERVER fail");        
      #endif    
      
      return 0;
  }  
  
  return 1;
}

void checkReset() {
  if (millis() - lastReset > resetEvery) {
    #ifdef DEBUG
      Serial.println("Resetting");
    #endif
    
    resetEsp8266();
//    configureEsp8266();
  } 
}

void resetCbuf(char cbuf[], int size) {
 for (int i = 0; i < size; i++) {
   cbuf[i] = 0; 
 } 
}

int clearSerial() {
  int count = 0;
  while (esp->available() > 0) {
    esp->read();
    count++;
  }
  
  return count;
}

#ifdef DEBUG
void printCbuf(char cbuf[], int len) {
  for (int i = 0; i < len; i++) {
      if (cbuf[i] <= 32 || cbuf[i] >= 127) {
        // not printable. print the char value
        Serial.print("(");
        Serial.print((uint8_t)cbuf[i]);
        Serial.print(")");
      } else {
        Serial.write(cbuf[i]);
      }
  } 
}
#endif

int readChars(char cbuf[], int startAt, int len, int timeout) {  
  int pos = startAt;
  long start = millis();

  while (millis() - start < timeout) {          
    if (esp->available() > 0) {
      uint8_t in = esp->read();
      
      if (in <= 32 || in >= 127 || in == 0) {
        // TODO debug print warning 
      }
      
      cbuf[pos++] = in;
      
      if (pos == len) {
        // null terminate
        cbuf[pos] = 0;
        return len;
      }    
    }
  }
  
  if (millis() - start >= timeout) {
    // timeout
    return -1; 
  }

  return pos;  
}

// debugging aid
int readFor(int timeout) {
  long start = millis();
  long lastReadAt = 0;
  bool waiting = false;
  long waits = 0;
  int pos = 0;
  
  #ifdef DEBUG
    Serial.println("<---");
  #endif
  
  resetCbuf(cbuf, BUFFER_SIZE);
  
  while (millis() - start < timeout) {          
    if (esp->available() > 0) {
      if (waiting) {
        waits++;
        waiting = false; 
      }
      
      uint8_t in = esp->read();
      cbuf[pos] = in;
      
      lastReadAt = millis() - start;
      pos++;
      
      if (in <= 32 || in >= 127) {
        // not printable. print the char value
        #ifdef DEBUG
          Serial.print("("); 
          Serial.print(in); 
          Serial.print(")");
        #endif
      } else {
        // pass through
        #ifdef DEBUG
          Serial.write(in);
        #endif
      }
    } else {
      waiting = true;
    }
  }
  
  // null terminate
  cbuf[pos] = 0;
  
  #ifdef DEBUG
    Serial.print("<--[");
    Serial.print(pos);
    Serial.print("c],[");
    Serial.print(lastReadAt);
    Serial.print("ms],[");
    Serial.print(waits);  
    Serial.println("w]");    
  #endif
  
  return pos;
}

int getCharDigitsLength(int number) {
    if (number < 10) {
      return 1;
    } else if (number < 100) {
      return 2;
    } else if (number < 1000) {
      return 3;
    }
  }  


void handleData() {
  //(13)(10)+IPD,0,4:hi(13)(10)(13)(10)OK(13)(10)
  
  #ifdef DEBUG
    Serial.println("\nGot data");
  #endif

  //ex +IPD,0,10:hi(32)again(13)
  // cbuf ,0,10:    
  
  // debug output
//  readFor(1000);
//  return;
  
  // serial buffer is at comma after D
  char* ipd = cbuf + readLen;
  
  // max channel + length + 2 commas + colon = 9
  int len = esp->readBytesUntil(':', ipd, 9);
  
  //Serial.print("read char to : "); Serial.println(len);
  // space ,0,1:(32)(13)(10)OK(13)(10)(13)(10)
  
  // parse channel
  // null term after channel for atoi
  ipd[2] = 0;
  int channel = atoi(ipd + 1);
  // reset
  ipd[2] = ',';
  
  #ifdef DEBUG
    Serial.print("Channel "); Serial.println(channel);
  #endif
  
  lastConnection = channel;
  
  //ipd[9] = 0;
  // length starts at pos 3
  len = atoi(ipd + 3);
  
  // subtract 2, don't want lf/cr
  len-=2;
  
  if (len <= 0) {
    #ifdef DEBUG
      Serial.println("no data");
    #endif
    return; 
  }
  
  // reset so we can print
  
  #ifdef DEBUG
    Serial.print("Data len "); 
    Serial.println(len);
  #endif
  
  if (len > 128) {
    // error.. too large
    #ifdef DEBUG
      Serial.println("Too long");
    #endif
    return;
  }

  // read input into data buffer
  int rlen = esp->readBytes(cbuf, len);
  
  if (rlen != len) {
    #ifdef DEBUG
      Serial.print("Data read failure "); 
      Serial.println(rlen);            
    #endif
    return;
  }
 
  // null terminate
  cbuf[len] = 0;
  
  #ifdef DEBUG
    Serial.print("Data:");  
    printCbuf(cbuf, len);
    Serial.println("");  
  #endif
          
  char response[] = "ok";
  
  // NOTE: print or write works for char data, must use print for non-char to print ascii
  
  int sendLen = strlen(response) + 2;
  
  // send AT command with channel and message length
  esp->print("AT+CIPSEND="); 
  esp->print(channel); 
  esp->print(","); 
  esp->print(sendLen); 
  esp->print("\r\n");
  
  //flush wrecks this device
  //esp->flush();
  
  // replies with
  //(13)(10)(13)(10)OK(13)(10)AT+CIPSEND=0,4(13)(13)(10)>(32)<--[27]
  // ctrl-c data results in
  //(253)(6)(13)(10)OK(13)(10)AT+CIPSEND=0,4(13)(13)(10)>(32)
  
  // should be same size everytime + send length                    
  int cmdLen = 26 + getCharDigitsLength(sendLen);
  
  int rCmdLen = readChars(cbuf, 0, cmdLen, 5000);
  
  if (rCmdLen == -1) {
    #ifdef DEBUG
      Serial.println("CIPSEND timeout");
    #endif
    
    return;
  } else if (rCmdLen != cmdLen) {
    #ifdef DEBUG
      Serial.print("Error unexp. reply len: "); 
      Serial.println(rCmdLen);
    #endif
    
    return;
  }
  
  if (strstr(cbuf, "busy") != NULL) {
    #ifdef DEBUG
      Serial.print("Busy");
    #endif
    
    return;
  } else if (strstr(cbuf, "OK") == NULL) {
    #ifdef DEBUG
      Serial.print("Error: ");
      Serial.println(cbuf);
    #endif
    
    return;
  }
  
  #ifdef DEBUG 
    Serial.println("CIPSEND reply");
    printCbuf(cbuf, cmdLen);
  #endif
  
  // send data to client
  esp->print(response);
  esp->print("\r\n");
  esp->flush();
  
  // reply
  //ok(13)(13)(10)SEND(32)OK(13)(10)<--[14]

  // fixed length reply
  // timed out a few times over 12h period with 1s timeout. increasing timeout to 5s
  len = readChars(cbuf, 0, strlen(response) + 12, 5000);
  
  if (len == -1) {
    #ifdef DEBUG 
      Serial.println("Data send timeout");            
    #endif
  } else if (len != 12 + strlen(response)) {
    #ifdef DEBUG 
      Serial.println("Reply len err");            
    #endif      
  }

  if (strstr(cbuf, "OK") == NULL) {
    #ifdef DEBUG     
      Serial.print("Error: ");
      Serial.println(cbuf);
    #endif
    return;
  }
  
  #ifdef DEBUG     
    Serial.println("Data reply");
    printCbuf(cbuf, len);        
  #endif
}

void handleConnected() {
  #ifdef DEBUG  
    Serial.println("Connected!");
  #endif
  // discard
  int len = esp->readBytesUntil(10, cbuf, BUFFER_SIZE - 1);
  // TODO parse channel
  // connected[channel] = true;
}

void handleClosed() {
  // TODO handle channel
  //0,CLOSED(13)(10)
  #ifdef DEBUG  
    Serial.println("Conn closed");
  #endif
  // consume line
  int len = esp->readBytesUntil(10, cbuf, BUFFER_SIZE - 1);
}

void loop() {
  //debugLoop();
  
  // the tricky part of AT commands is they vary in length and format, so we don't know how much to read
  // with 6 chars we should be able to identify most allcommands
  if (esp->available() >= readLen) {
    #ifdef DEBUG      
      Serial.print("\n\nSerial available "); 
      Serial.println(esp->available());
    #endif
    
    readChars(cbuf, 0, readLen, 1000);
    
    #ifdef DEBUG
      printCbuf(cbuf, readLen);
    #endif
    
    // not using Serial.find because if it doesn't match we lose the data. so not helpful
    
    if (strstr(cbuf, "+IPD") != NULL) {
      //(13)(10)+IPD,0,4:hi(13)(10)(13)(10)OK(13)(10)      
      handleData();
    } else if (strstr(cbuf, ",CONN") != NULL) {
      //0,CONNECT(13)(10)      
      handleConnected();
    } else if (strstr(cbuf, "CLOS") != NULL) {
      lastConnection = -1;
      handleClosed();
    } else {
      #ifdef DEBUG
        Serial.println("Unexpected..");
      #endif
      
      readFor(2000);
      
      // assume the worst and reset
      resetEsp8266();
    }
    
    if (false) {
      // health check        
      if (sendAt() != 1) {
        
      }
    }
    
    resetCbuf(cbuf, BUFFER_SIZE);
    // discard remaining. should not be any remaining
    clearSerial();      
  }
  
  checkReset();  
}