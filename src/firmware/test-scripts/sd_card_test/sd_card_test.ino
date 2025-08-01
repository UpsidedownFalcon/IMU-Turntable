#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#define SD_CS        BUILTIN_SDCARD  // chip select for the onboard SD socket
const char* TRAJ_DIR = "/traj";

enum RxState {
  IDLE,
  RECEIVING
};

RxState rxState = IDLE;
String headerLine;
char   currentAxis      = 0;
uint32_t bytesRemaining = 0;
File    recvFile;

void setup() {
  // --- Serial over USB ---
  Serial.begin(115200);
  // while (!Serial) ;           // wait for USB Serial to come up
  Serial.println("Teensy SD Receiver starting...");

  // --- SD card init ---
  if (!SD.begin(SD_CS)) {
    Serial.println("SD.begin() failed!");
    while (true) delay(10);
  }
  // ensure the /traj directory exists
  if (!SD.exists(TRAJ_DIR)) {
    if (!SD.mkdir(TRAJ_DIR)) {
      Serial.println("Failed to create /traj directory!");
      while (true) delay(10);
    }
  }
  Serial.println("Ready to receive trajectories.");

  pinMode(13, OUTPUT); 
  digitalWrite(13, HIGH); 
}

void loop() {
  // Process incoming bytes
  while (Serial.available()) {
    char c = Serial.read();

    if (rxState == IDLE) {
      // accumulate until newline
      if (c == '\n') {
        headerLine.trim();
        parseHeader(headerLine);
        headerLine = "";
      } else {
        headerLine += c;
      }
    }
    else if (rxState == RECEIVING) {
      // read as many bytes as available (up to bytesRemaining)
      size_t toRead = min((size_t)Serial.available(), (size_t)bytesRemaining);
      uint8_t buf[64];
      while (toRead > 0) {
        size_t chunk = min(toRead, sizeof(buf));
        size_t got   = Serial.readBytes(buf, chunk);
        recvFile.write(buf, got);
        bytesRemaining -= got;
        toRead        -= got;
      }
      // done?
      if (bytesRemaining == 0) {
        recvFile.close();
        Serial.println("OK");   // acknowledge
        rxState = IDLE;
      }
    }
  }
}

// Parse lines like: "BEGIN X 123456"
void parseHeader(const String& line) {
  if (!line.startsWith("BEGIN ")) return;

  // tokenize
  // expected tokens: ["BEGIN", "<Axis>", "<size>"]
  int firstSpace  = line.indexOf(' ');
  int secondSpace = line.indexOf(' ', firstSpace+1);
  if (firstSpace < 0 || secondSpace < 0) return;

  String axisStr = line.substring(firstSpace+1, secondSpace);
  String sizeStr = line.substring(secondSpace+1);

  if (axisStr.length() != 1) return;
  char axis = axisStr.charAt(0);
  if (axis!='X' && axis!='Y' && axis!='Z') return;

  uint32_t sz = sizeStr.toInt();
  if (sz == 0) return;

  // open the file for writing (overwrite any old file)
  String path = String(TRAJ_DIR) + "/" + axis + ".bin";
  recvFile = SD.open(path.c_str(), FILE_WRITE);
  if (!recvFile) {
    Serial.print("ERR: could not open ");
    Serial.println(path);
    return;
  }

  currentAxis      = axis;
  bytesRemaining   = sz;
  rxState          = RECEIVING;
  Serial.print("RX ");
  Serial.print(axis);
  Serial.print(" ");
  Serial.println(sz);
}
