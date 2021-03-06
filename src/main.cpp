/*

Bronco Racing 2020 Dyno zDAQ - See README for details

*/

#include <Hx711.h>
#include <PID.h>
#include <mbed.h>
#include <servo.h>

CAN can0(PA_11, PA_12, 250000);

PID controller(2.75 , 100000, 0, 80);

Hx711 scale1 = Hx711(D11, D9);

Serial usb = Serial(USBTX, USBRX, 921600);

DigitalOut led(LED3);
DigitalOut testPin(D12); // used for timing tests
DigitalOut solenoidPin(D1);

volatile uint16_t rpm = 0;
volatile int16_t waterTemp = 0;
volatile double scaleValue = 0;
volatile uint32_t scaleInt = 0;

double servoMin = 5; // calibrated servo max / min values
double servoMax = 135;
volatile int servoVal = servoMin;
float RPMSet = 7000;
float RPMBuff = 1000;
volatile float RPMLast = 0;
volatile double RPMInt = 0;
volatile float servoValP = 0;
volatile float servoValI = 0;
volatile float servoValD = 0;

volatile double Kp = 0.03;
float Ki = 0.0006; //0.001
float Kd = 1; // 0.2

volatile float torque = 0;
volatile float hp = 0;
float gearCal = 1.093;


CANMessage inMsg;
CANMessage outMsg;

Timer dataTimer;
Timer canTimer;

Servo servo(A1);

void CANCallback();

int main()
{

  dataTimer.start();

  controller.setInputLimits(0.0, 14000);
  controller.setOutputLimits(servoMin, servoMax);
  controller.setMode(1);
  controller.setSetPoint(RPMSet);

  canTimer.start();

  led.write(0);

  outMsg.id = 0;
  outMsg.len = 8;

  for (int i = 0; i < outMsg.len; i++)
  {
    outMsg.data[i] = 0;
  }

  scale1.setMultiplierA(1.f);
  scale1.set_scale(0.001f);

  wait_ms(100);

  scale1.tareA(5, .2); // Tare 5 times, .2 seconds each, and average them

  wait_ms(100);
  led.write(1);

  can0.filter(0x0CFFF048, 0x0CFFF548, CANExtended, 0);
  can0.attach(CANCallback);

  while (1)
  {
    // Read scale and remove decimal point. Remove negative vals.
    scaleInt = (uint32_t)((abs(scale1.readTaredA())) * 1000);
    controller.setProcessValue((float)rpm);

    // if (rpm > 1)
    // {
    //   servoVal = servoMax - controller.compute();
    // }
    // else
    // {
    //   servoVal = servoMin;
    // }

    if ((rpm + RPMBuff) > RPMSet)
    {
      servoValP  = Kp * ((rpm + RPMBuff) - RPMSet);
    }
    else
    {
      servoValP = servoMin;
    }
    
    if (rpm > (RPMSet - RPMBuff))
    {
      RPMInt = RPMInt + (rpm - RPMSet);
    }
    else
    {
      RPMInt = 0;
    }
    
    servoValI = Ki * RPMInt;
    
    servoValD = Kd * (rpm - RPMLast);

    servoVal = servoValP + servoValI + servoValD;

    if (servoVal > servoMax)
    {
      servoVal = servoMax;
    }

    if (servoVal < servoMin)
    {
      servoVal = servoMin;
    }
    
    servo.write(servoVal); 
    RPMLast = rpm;

    // // for servo calibration
    // servoVal = servoVal + 1;
    // if (servoVal > 135)
    // {
    //   servoVal = 5;
    // }
    

    // Send CAN alive frame
    if (canTimer.read_ms() > 50)
    {
      can0.write(outMsg);
      canTimer.reset();
    }


    //calibrate output
    // zeroVal = (41.142 * rpm) + 63664;
    // zeroVal = 0;
    torque = (2.1393 * (scaleInt / 10000) + 5.1861)/gearCal;
    hp = torque * rpm / 5252;

    // // Send Data for plotting
    // usb.printf("%f,", dataTimer.read());
    // usb.printf("%d,", (rpm / 100));
    // usb.printf("%f,", torque);
    // usb.printf("%f\n", hp);

    // For terminal viewing
    usb.printf("%f\t", servoValP);
    usb.printf("%f\t", servoValI);
    usb.printf("%f\t", servoValD);
    usb.printf("%d\t", (servoVal)); // corrected valve position
    usb.printf("%d\t", rpm);
    usb.printf("%ld\n", scaleInt);
  }
}

// Fires on incoming CAN frame interrupt
void CANCallback()
{
  if (can0.read(inMsg))
  {
    // Read in ECU frames
    // PE1
    if (inMsg.id == 0x0CFFF048)
    {
      rpm = ((inMsg.data[1] << 8) + inMsg.data[0]);
      led = !led;
    }
    // PE6
    else if (inMsg.id == 0x0CFFF548)
    {
      uint16_t newTemp = ((inMsg.data[5] << 8) + inMsg.data[4]);
      if (newTemp > 32767)
      {
        newTemp = newTemp - 65536;
      }
      // waterTemp = (uint16_t)((newTemp / 10) * 1.8) + 32;
    }
  }
}