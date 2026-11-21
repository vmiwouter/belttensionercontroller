/*
 * =========================================================================
 *  Belt Tensioner Controller (v1) - SimHub Binary Firmware
 *  Author   : vmiwouter (https://github.com/vmiwouter)
 *  Board    : Arduino Leonardo (ATmega32U4) + DFRobot GP8413 15-bit DAC
 *  Protocol : SimHub Motion Generic Serial (Binary, 15-bit 0..32767)
 *             Update command : <3><0><Axis1><Axis2>
 *             Stop command   : <3><255>
 * =========================================================================
 */

#include <Wire.h>
#include <DFRobot_GP8XXX.h>

// =========================================================================
// 1. HARDWARE DEFINITIONS & SETTINGS
// =========================================================================
#define DAC_ADDR 0x58                        // Default I2C address of DFRobot GP8413
const int EnablePin = A0;                    // Servo Enable output (HIGH = disabled, LOW = enabled)
const unsigned long WATCHDOG_TIMEOUT = 1000; // Safe shutdown timeout when SimHub stops (ms)

DFRobot_GP8413 GP8413(DAC_ADDR);

// =========================================================================
// 2. ZERO-COPY BINARY PACKET STRUCTURE (6 Bytes: <3><0> + 2x 16-bit)
// =========================================================================
union SimHubPacket {
  struct {
    uint8_t  header;                         // Byte 0: Frame sync header (<3>)
    uint8_t  validation;                     // Byte 1: <0> = Motion update, <255> = Stop Motion
    uint16_t belt1;                          // Bytes 2..3: BELT 1 (15-bit Big-Endian)
    uint16_t belt2;                          // Bytes 4..5: BELT 2 (15-bit Big-Endian)
  } data;
  uint8_t raw[6];                            // Raw byte buffer sharing the exact same RAM address
};

SimHubPacket packet;
uint8_t byteCount = 0;
bool isMotorEnabled = false;
unsigned long lastPacketTime = 0;

// =========================================================================
// 3. SETUP
// =========================================================================
void setup() {
  pinMode(EnablePin, OUTPUT);
  digitalWrite(EnablePin, HIGH);             // Hard lock: keep servo drives disabled at boot

  Serial.begin(115200);
  
  Wire.begin();
  Wire.setWireTimeout(3000, true);           // Automatic I2C bus recovery after EMI spikes

  while (GP8413.begin() != 0);               // Wait until DAC module responds on the I2C bus
  GP8413.setDACOutVoltage(0, 0);             // Ensure analog belt outputs start at 0.00V
  GP8413.setDACOutVoltage(0, 1);
}

// =========================================================================
// 4. MAIN LOOP
// =========================================================================
void loop() {
  // --- SAFETY WATCHDOG (Serial Timeout or I2C EMI Fault) ---
  if (isMotorEnabled) {
    if ((millis() - lastPacketTime > WATCHDOG_TIMEOUT) || Wire.getWireTimeoutFlag()) {
      digitalWrite(EnablePin, HIGH);         // Instantly de-energize servo drives (safe mode)
      isMotorEnabled = false;
      
      Wire.clearWireTimeoutFlag();           // Clear hardware flag to allow clean recovery
      GP8413.setDACOutVoltage(0, 0);         // Instantly drop analog signals to 0.00V
      GP8413.setDACOutVoltage(0, 1);
      byteCount = 0;
    }
  }

  // --- ZERO-LATENCY BINARY PROTOCOL PARSER ---
  while (Serial.available() > 0) {
    uint8_t c = Serial.read();
    
    // Frame sync: Drop bytes until header <3> is located
    if (byteCount == 0 && c != 3) { continue; }

    packet.raw[byteCount] = c;
    byteCount++;

    // Detect <3><255> Stop Motion command from SimHub for instant release
    if (byteCount == 2 && packet.data.validation == 255) {
      if (isMotorEnabled) {
        digitalWrite(EnablePin, HIGH);
        isMotorEnabled = false;
        GP8413.setDACOutVoltage(0, 0);
        GP8413.setDACOutVoltage(0, 1);
      }
      byteCount = 0;
      continue;
    }

    // Process complete 6-byte motion update frame
    if (byteCount == 6) {
      if (packet.data.validation == 0) {
        lastPacketTime = millis();

        // Safe start interlock: Enable servo drives only when active motion arrives
        if (!isMotorEnabled) { 
          digitalWrite(EnablePin, LOW); 
          isMotorEnabled = true;
        }

        // Convert Big-Endian serial bytes to Little-Endian 15-bit integers
        uint16_t belt1 = (packet.data.belt1 >> 8) | (packet.data.belt1 << 8);
        uint16_t belt2 = (packet.data.belt2 >> 8) | (packet.data.belt2 << 8);

        // Direct library write: Keeps steady I2C pacing for ATmega32U4 USB stability
        GP8413.setDACOutVoltage(belt1, 0);
        GP8413.setDACOutVoltage(belt2, 1);
      }
      byteCount = 0;
    }
  }
}
