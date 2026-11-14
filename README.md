# AASD-15 / S2-SVD15 Advanced Torque-Based Belt Tensioner Guide

**WARNING: USE AT YOUR OWN RISK**

## Acknowledgements & Credits
Special thanks to the pioneers and creators of this hardware architecture:
- **OpenSFX project:** The fundamental root of the 3D-printed linear actuator mechanics.  
  URL: https://opensfx.com
- **Hoernle:** For his brilliant mechanical belt tensioner design and detailed build instructions.  
  URL: https://www.overtake.gg/threads/hoernles-belt-tensioner-detailed-build-instructions.249951/
- **Pyronious:** For simplifying the DB25 interface via his open-source Arduino Leonardo breakout shield.  
  URL: https://github.com/Pyronious/SFX-100-Breakout-Board
- **Jonas Lang:** First concept and implementation thread using AC servo torque-based control with an external DAC.  
  URL: https://forum.virtualracing.org/threads/aasd-15-drehmomentsteuerung-fuer-belt-tensioner.139794
- **Jakob (irTactile):** For providing deep protocol insights via his advanced hardware controller.  
  URL: https://github.com/jSchmidt-design/irtactile-belt

---

## Hardware Modification (Torque Mode Transition)
By soldering a tiny **DFRobot GP8413 2-channel I2C DAC module** onto the underside of your original Pyronious shield, you can bypass position-based travel entirely and drive your actuators using pure analog **Torque Mode**.

Because the GP8413 module is so compact, it fits perfectly beneath the PCB, allowing you to reuse your original 3D-printed controller housing. Secure the DAC with electrical tape at the bottom right inside the case to guide the wires safely.

*⏱️ **Installation Time:** This modification requires soldering only 8 short jumper wires and takes approximately 15 minutes to complete for anyone with basic soldering skills.*

*🔄 **Reversibility:** No permanent cuts are made to the shield. You can always flash your Arduino Leonardo back to the original SimFeedback firmware to return to position mode.*

### Interfacing & Wiring
![schema](./images/schemafinal.png)

Connect the GP8413 module to the Arduino Leonardo I2C lines and DB25 breakouts as follows:

**Digital I2C Input (Breakout Shield to DAC):**
- **C** ──► Connect to SFX Shield SCL
- **D** ──► Connect to SFX Shield SDA
- **-** ──► Connect to SFX Shield GND
- **+** ──► Connect to SFX Shield +5V

**Analog Output (DAC to DB25 Rails):**
- **VOUT 0 (Channel 0)** ──► Connect to Motor 1 DB25 PIN 25 (Vref Analog Input)
- **VOUT 1 (Channel 1)** ──► Connect to Motor 2 DB25 PIN 25 (Vref Analog Input)
- **GND (Analog)** ──► Connect to BOTH DB25 breakouts PIN 13 (GND/AGND Signal Ground). *(Note: You can bridge Pin 10 to Pin 13 if you are not using an SFX Shield).*

---

## 🛠️ AASD-15 Servo Driver Parameters
Original position-mode parameters (such as electronic gear ratios or pulse scaling) are completely ignored in torque mode.

Configure **both** servo driver displays to the following recommended settings:
- **Pn002 = 0** ──► Control Mode Selection: Hard-locked in Torque Mode
- **Pn008 = 300** ──► Internal Torque Limit (Forward)
- **Pn009 = -300** ──► Internal Torque Limit (Reverse)
- **Pn188 = 5** ──► Analog Input Filter Time (0.1 ms units). Increase up to 20 max if you encounter heavy EMI noise or belt vibrations.
- **Pn189 = 30** ──► Analog Torque Command Gain. Set to 30 to perfectly scale 300% maximum peak torque over the full 10.0V DAC output.
- **Pn191 = 0 (or 1)** ──► Change this parameter if you need to invert your physical motor rotation direction.
- **Pn198 = 150** ──► Speed Limit During Torque Control: Hard-capped at 150 RPM for essential mechanical safety when unburdened.

*💡 **DEBUG TIP:** Use the monitoring parameter `dn16` (Analog torque command voltage) on the driver's LED display to see the exact incoming analog voltage from the DAC in real time for diagnostic verification.*

*\*Note: These parameters have been fully tested and verified for AASD-15 and S2-SVD15 servo drives. If you use any other brand or model of servo drive, cross-reference and match these parameters according to their specific hardware manual. For non-DB25 drives, you cannot use the SFX breakout shield and must wire everything manually by hand—making the installation significantly more complicated compared to adding the 8 short jumper wires required for this shield modification.*

---

## 💻 Arduino Firmware (Sketch)
The required Arduino sketch is included directly in this repository:  
👉 **[Download / View Arduino Sketch (`sketch.ino`)](./sketch.ino)**

### Key Features
- **Official DFRobot Libraries:** Built directly on the official **`DFRobot_GP8XXX`** libraries for reliable I2C communication and full 15-bit hardware DAC control.
- **Seamless SimHub Passthrough:** Acts as a low-latency, direct passthrough bridge that translates incoming SimHub serial telemetry frames straight into smooth analog torque voltages without extra translation layers or middleware.

### Flashing Instructions
1. Open **`sketch.ino`** in the **Arduino IDE**.
2. Install the official **`DFRobot_GP8XXX`** library via the Arduino Library Manager.
3. Go to **Tools > Board** and select **Arduino Leonardo**.
4. Select the correct **COM Port** and click **Upload**.

---

## ⚙️ SimHub Configuration & Tuning

### 1. Custom Generic Serial Controller Settings
In the SimHub hardware/serial configuration menu, apply the following communication parameters:
- **Flow Control:** Turn RTS and DTR completely OFF
- **Axis output format:** Binary
- **Axis resolution (Bit range):** 15 (Direct 0 to 32767 scale mapping)
- **Motion update commands:** `<3><0><Axis1><Axis2>`

### 2. SimHub Belt Calibration Wizard Settings
To achieve a highly realistic "No Release" setup—ensuring a constant, stable belt preload on straights without artificial slack or loosening during acceleration—use the following values in the SimHub calibration wizard:

1. **Default Belt Center Position (Step 1):**
   * **Value:** Typically between `2%` and `8%` preload depending on your specific setup
   * *Purpose:* In torque mode, the Center Position acts directly as your constant baseline preload against your chest.

2. **Maximum Pull Position (Step 2):**
   * **Value:** Recommended range between `20%` and `50%` (tuning advice: start testing from `20%` and gradually increase in steps of `5%`. Note: exceeding this range or pulling too high can cause serious shoulder pain).
   * *Purpose:* Defines the upper limit of maximum pulling force during heavy braking zones and cornering forces.

3. **Maximum Release Position (Step 3):**
   * **Value:** `0%`
   * *Purpose:* Enforces the **"No Release"** profile. Prevents the belts from slackening or dropping below the baseline during acceleration on straightaways.

4. **Parking Position (Step 4):**
   * **Value:** `0%`
   * *Purpose:* Keeps the system completely relaxed or zeroed when sitting idle in the pit lane.

### 3. Final Working Range Overview
Once the calibration wizard is saved, your SimHub **Working Range** overview will reflect:
* **Center position:** `2%–8%` (acts as preload in torque mode)
* **Minimum position:** `2%–8%` (minimum equals preload, but you can always manual override it for a lower value to your preference)
* **Maximum position:** `20%~50%`
* **Park position:** `0%`

---

## 🏎️ 4. Baseline Telemetry Tuning (Surge & Sway)
*ℹ️ **Note:** The tuning parameters below are intended strictly as a safe starting baseline. Because every rig, harness geometry, and personal preference is different, these effects can be endlessly tweaked, expanded, and fine-tuned by the user inside SimHub.*

### Surge Effect (Primary Dominance)
- **Input Limit:** 10.0 to 15.0 m/s² (depending on car)
- **Motion Range:** 100%
- **Smoothing:** 10 (Sharp, instantaneous reaction to deceleration telemetry)
- **Belt Release:** No belt release or release a little (5%)

### Sway Effect (Lateral Support Headroom)
- **Input Limit:** 9.0 to 20.0 m/s² (depending on car)
- **Motion Range:** 75% (Can be increased up to 90% if the lateral effect feels too weak)
- **Smoothing:** 10~20 (Smooth, progressive build-up through cornering phases)
- **Belt Release:** No belt release or release a little (15%)
