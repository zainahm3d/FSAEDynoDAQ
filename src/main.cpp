/*

Bronco Racing 2020 Dyno zDAQ

Tasks:
  - HX711 2 channel scale amplifier input (80sps)
  - Servo motor control
  - CAN bus alive frame generation (50ms)
  - CAN bus data interpretation
  - Data transfer to MATLAB over serial

Info:
  - Current output rate is 80sps clocked by the HX711 chip.
  - Serial routed to Nucleo L432KC onboard micro USB port
  - Output multiplies HX711 reading by 1000 (keeps 3 decimal places)
*/

#include <Hx711.h>
#include <mbed.h>

CAN can0(PA_11, PA_12, 250000);

Hx711 scale1 = Hx711(D11, D9);

Serial usb = Serial(USBTX, USBRX, 921600);

DigitalOut led(LED3);
DigitalOut testPin(D12); // used for timing tests

volatile uint16_t rpm = 0;
volatile int16_t waterTemp = 0;
volatile double scaleValue = 0;
volatile uint32_t scaleInt = 0;

CANMessage inMsg;
CANMessage outMsg;

Timer canTimer;

int main() {

  canTimer.start();

  led.write(0);

  outMsg.id = 0;
  outMsg.len = 8;

  for (int i = 0; i < outMsg.len; i++) {
    outMsg.data[i] = 0;
  }

  scale1.setMultiplierA(1.f);
  scale1.set_scale(0.001f);

  wait_ms(100);

  // scale1.tareA(5, 1); // Tare 5 times, 1 second each, and average them

  wait_ms(100);
  led.write(1);

  while (1) {
    if (can0.read(inMsg)) {
      // Read in ECU frames
      // PE1
      if (inMsg.id == 0x0CFFF048) {
        rpm = ((inMsg.data[1] << 8) + inMsg.data[0]);
        led = !led;
      }
      // PE6
      else if (inMsg.id == 0x0CFFF548) {
        uint16_t newTemp = ((inMsg.data[5] << 8) + inMsg.data[4]);
        if (newTemp > 32767) {
          newTemp = newTemp - 65536;
        }
        waterTemp = ((newTemp / 10.0) * 1.8) + 32;
      }
    }

    // Read scale and remove decimal point. Remove negative vals.
    scaleInt = (uint32_t)((scale1.readA()) * 1000);
    if (scaleInt < 0) {
      scaleInt = 0;
    }

    // Print CAN alive frame
    if (canTimer.read_ms() > 50) {
      can0.write(outMsg);
      canTimer.reset();
    }

    // Send Data
    usb.printf("r%d", rpm);
    usb.printf("l%d\n", scaleInt);
  }
}
