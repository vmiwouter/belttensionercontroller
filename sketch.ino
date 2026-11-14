#include <Wire.h>
#include <DFRobot_GP8XXX.h>

#define DAC_ADDR 0x58 
const int EnablePin = A0; // Hardware safety lines on Pyronious shield layout

DFRobot_GP8413 GP8413(DAC_ADDR);

// BINARY UNION: Bytes and 16-bit variables share the exact same hardware memory address
union SimHubPacket {
  struct {
    uint8_t header;       // Byte 0: Always <3>
    uint8_t validation;   // Byte 1: Always <0>
    uint16_t channel0;    // Bytes 2 & 3: Loaded directly into memory as a 16-bit type
    uint16_t channel1;    // Bytes 4 & 5: Loaded directly into memory as a 16-bit type
  } data;
  uint8_t raw[6];         // 6-byte raw serial buffer array
};

SimHubPacket packet;
uint8_t byteCount = 0;
bool isMotorEnabled = false; // Superfast memory flag replacing slow digitalReads
unsigned long lastPacketTime = 0;  
const unsigned long WATCHDOG_TIMEOUT = 1000; // Safe shutdown window (1 second)

void setup() {
  pinMode(EnablePin, OUTPUT);
  digitalWrite(EnablePin, HIGH); // Hard lock: Keep motors completely off and safe during bootup
  Serial.begin(115200);
  
  Wire.begin();
  Wire.setWireTimeout(3000, true); // Automatic I2C bus recovery after EMI spikes

  while (GP8413.begin() != 0); // Wait until DAC module is located on the I2C bus
}

void loop() {
  // --- AUTOMATIC HARDWARE DISCONNECT WATCHDOG ---
  // Instantly executes via memory flag if SimHub goes silent OR if an EMI I2C timeout occurs
  if (isMotorEnabled) {
    if ((millis() - lastPacketTime > WATCHDOG_TIMEOUT) || Wire.getWireTimeoutFlag()) {
      digitalWrite(EnablePin, HIGH); // Instantly de-energize servo loops (safe mode)
      isMotorEnabled = false;        // Toggle state variable
      
      Wire.clearWireTimeoutFlag();   // Clear the hardware flag to allow future recovery
      GP8413.setDACOutVoltage(0, 0); // Instantly drop analog signals straight to 0.00V
      GP8413.setDACOutVoltage(0, 1);
      byteCount = 0;
    }
  }

  // --- ZERO-LATENCY BINARY PROTOCOL PARSER ---
  while (Serial.available() > 0) {
    uint8_t c = Serial.read();
    
    if (byteCount == 0 && c != 3) { continue; } // Frame sync: Drop bytes until header <3> is located

    packet.raw[byteCount] = c; // Stream the byte directly into its final physical RAM destination
    byteCount++;

    if (byteCount == 6) {
      if (packet.data.validation == 0) { // Confirm inner header validation token <0>
        lastPacketTime = millis(); // Refresh watchdog timer

        // SAFE START INTERLOCK: Only enable the servo drives if SimHub is actively broadcasting motion data
        if (!isMotorEnabled) { 
          digitalWrite(EnablePin, LOW); 
          isMotorEnabled = true;
        }

        // BIG-ENDIAN REALTIME RECONSTRUCTION: Swaps bytes directly using shared RAM layouts (No slow pointer calculations)
        uint16_t ch0 = (packet.data.channel0 >> 8) | (packet.data.channel0 << 8);
        uint16_t ch1 = (packet.data.channel1 >> 8) | (packet.data.channel1 << 8);

        // DIRECT LIBRARY WRITE: Low-latency register execution
        GP8413.setDACOutVoltage(ch0, 0); // Write 15-bit value to Channel 0
        GP8413.setDACOutVoltage(ch1, 1); // Write 15-bit value to Channel 1
      }
      byteCount = 0; // Reset packet frame index counter for the next loop payload
    }
  }
}
