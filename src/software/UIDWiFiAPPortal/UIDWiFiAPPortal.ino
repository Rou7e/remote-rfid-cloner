/* AP Captive Portal for reading/writing UIDs on RFID chips */
/* Made by Dmitry Abramov */
/* git: https://github.com/Rou7e/remote-rfid-cloner */

#include <WiFi.h>
#include <DNSServer.h>
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN   4     // Configurable, see typical pin layout above
#define SS_PIN    21    // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance

MFRC522::MIFARE_Key key;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WiFiServer server(80);

byte NEW_UID[4] = {0xFF, 0xFF, 0xFF, 0xFF};

bool currentlyWriting = false;

String responseHTML1 = ""
  "<!DOCTYPE html><html><head><title>RFID Tools Portal</title></head><body>"
  "<h1>RFID Tools Portal</h1><p>Currently writing UID: ";

String responseHTML1d5 = "<p> Current operation: </p>";
  
String responseHTML2 = "</p><p>Enter UID to write on every card:</p>"
"   <form action='/action_page.php'>"
"  <label for='write'>UID to send:</label><br>"
"  <input type='text' id='write' name='write' value='FF-FF-FF-FF'><br>"
"  <input type='submit' value='Write UID'>"
"</form><p id='readm'>Read RFID without rewriting UID</p>"
"  <a href='/readm'>Read Mode</a>"
"<p>Last read result: </p>"
"</body></html>";

String readResponse = "";

String successForm = "<!DOCTYPE html><html><head><title>RFID Tools Portal</title></head><body>"
"<h1>Thanks!</h1>"
"<p>New UID accepted!</p>"
"<a href='/'>Back to captive portal</a>"
"</body></html>";

void setup() { 
  WiFi.disconnect();   //added to start with the wifi off, avoid crashing
  WiFi.mode(WIFI_OFF); //added to start with the wifi off, avoid crashing
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("NFC UID Assistant", "rfidmaster");
  SPI.begin();         // Init SPI bus
  mfrc522.PCD_Init();  // Init MFRC522 card
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.begin(115200);
  server.begin();
  pinMode(2,OUTPUT);
}

void loop() {
  dnsServer.processNextRequest();
  WiFiClient client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
    Serial.println("New Client.");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print(responseHTML1);
            String strUID;
            for (int i=0; i<4; i++) {strUID+=String(NEW_UID[i], HEX);}
            client.print(strUID);
            client.print(responseHTML1d5);
            if (currentlyWriting) {client.print("writing UID!"); digitalWrite(2, HIGH);}
            else {client.print("reading RFID!"); digitalWrite(2, LOW);}
            client.print(responseHTML2);
            client.print(readResponse);
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
        if (currentLine.indexOf("readm") >= 0) {
          currentlyWriting=false;
        }
        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.indexOf("write") >= 0) {
          String UIDLine = currentLine.substring(currentLine.indexOf("write")+6);
          if (UIDLine.length() == 11) {
            Serial.println(UIDLine);
            char cUIDLine[12];
            UIDLine+="-";
            UIDLine.toCharArray(cUIDLine,12);
            sscanf(cUIDLine, "%x-%x-%x-%x-", &NEW_UID[0], &NEW_UID[1],&NEW_UID[2],&NEW_UID[3]);
            currentlyWriting=true;
          }
          
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }

  // writer block
  if (currentlyWriting) {
      // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle. And if present, select one.
  if ( ! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial() ) {
    return;
  }
  
  // Now a card is selected. The UID and SAK is in mfrc522.uid.
  
  // Dump UID
  Serial.print(F("Card UID:"));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  } 
  Serial.print(F("New UID:"));
  for (byte i = 0; i < 4; i++) {
    Serial.print(NEW_UID[i] < 0x10 ? " 0" : " ");
    Serial.print(NEW_UID[i], HEX);
  } 
  Serial.println();

  // Set new UID

  if ( mfrc522.MIFARE_SetUid(NEW_UID, (byte)4, true) ) {
    Serial.println(F("Wrote new UID to card."));
  }
  
  // Halt PICC and re-select it so DumpToSerial doesn't get confused
  mfrc522.PICC_HaltA();
  if ( ! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial() ) {
    return;
  }
  
  // Dump the new memory contents
  Serial.println(F("New UID and contents:"));
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
  
  delay(2000);
  } else { // reader block
  if ( ! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial() ) {
    return;
  }
  readResponse="";
  readResponse+=(F("Card UID:"));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    readResponse+=(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    readResponse+=(mfrc522.uid.uidByte[i], HEX);
  } 
  readResponse+="\n";
  }
  delay(2000);
}


int StrToHex(char str[])
{
  return (int) strtol(str, 0, 16);
}
