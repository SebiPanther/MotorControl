/*
 * This is a "small" arduino sketch to control two hoverboard ESCs with one nunchuck.
 * At the moment this runs on a arduino with the following IOs:
 * A4/A5 - Nunchuck connector
 * D8/D9 - Software serial for the first ESC
 * D10/D11 - Software serial for the secound ESC. This also delivers power to the board over a step down conveter. Don't connect usb and ESC at the same time!
 * D12 - Switch to enable or disable steering. Thats more important for railcar mode. To steer on rails is a waiste of power
 * In reason of the bad signals from the nuncuck its not possible to map the nunchuck values to the ESC values.
 * The Nunchuck gives only the following informations:
 * Joystick Y: 0-255 (Maybe 16steps)
 * Joystick X: 0-255 (Maybe 16steps)
 * ZButton Pressed
 * CButton Pressed
 * Also the Joystick of the nunchuck is "kind of" a circle and not a square.
 * The ESC(s) needs the following informations:
 * Speed: -1000 - +1000
 * Steer: -1000 - +1000
 * Thats one of the reason for the complexity of the code.
 * Also it has some more features:
 * - Deadman switch - do not drive until the ZButton is pressed.
 * - Only start driving if deadman switch is pressed and the joystick is in the middle
 * - Accelerate on the maximum positions on the nunchuck and only then!
 * - Keep the current speed if you dail the position of the joystick slightly back from its maximum position 
 * - Break if the joystick is moved in the opposite position until it stops.
 * - Don't drive in another direction until the joystick reached the middle postion one more time. 
 * - Its possible to steer from one to another side without disruption.
 */
#include <SoftwareSerial.h>
#include <NintendoExtensionCtrl.h>
#include "math.h"


// ########################## DEFINES ##########################
#define HOVER_SERIAL_BAUD   38400       // [-] Baud rate for HoverSerial (used to communicate with the hoverboard)
#define SERIAL_BAUD         115200      // [-] Baud rate for built-in Serial (used for the Serial Monitor)
#define START_FRAME         0xAAAA       // [-] Start frme definition for reliable serial communication
#define TIME_SEND           100         // [ms] Sending time interval
#define SPEED_MAX_TEST      300         // [-] Maximum speed for testing

//Struct that described all the static values
typedef struct{
  const double YMiddle = 133.5; //Depense on your Nunchuck - Middle Position of the Y Achses
  const double ZMiddle = 122.5;
  const double MiddleCircle = 7; //3.5;
  const double InnerCircle = 80.0;
  const double EndOfOuterCircle = 115.0;
  const float SpeedOverallMax = 1000.0;
  const float SpeedOverallZero = 0.0;
  const float SpeedOverallMin = -1000.0;
  const float SpeedIncreasement = 6.0;
  const float SpeedDecreasementNormal = 6.0;
  const float SpeedDecreasementFast = 15.0;
  const float SpeedDecreasementMax = 30.0;
  const float SteerOverallRight = 1000.0;
  const float SteerOverallZero = 0.0;
  const float SteerOverallLeft = -1000.0;
  const float SteerIncreasement = 15.0;
  const float SteerDecreasementNormal = 15.0;
  const float SteerDecreasementFast = 15.0;
  const float SteerDecreasementMax = 15.0;
  const int Delay = 10;
} ConstStruct;

//Struct that described  the State of the Nunchuck
typedef struct{
  boolean Connected = false; //Nunchuck connected
  int JoyY = 127; //Forward/Backward
  int JoyX = 127; //Left/Right
  boolean ZButton = false; //Dead man switch
  boolean CButton = false; //Additional Switch
  boolean DeadManSwitchPress = false; //Dead man switch
} NunchukStruct;

//Enum that describe the direction of travel
//This important to middle the nunchuck before drive in the other direction
enum DrivingDirectionEnum {
  ForwardDriving,
  NoDriving,
  BackwardDriving
};

//Struct that describe the speed and the steer Value
typedef struct{
  int Speed = 0;
  int Steer = 0;
  bool SpeedHandled = false;
  bool SteerHandled = false;
  DrivingDirectionEnum DrivingDirection = NoDriving;
} DriveStruct;

//Enum that describe the steer postion
enum NunchuckSteerEnum {
  MiddleSteerPosition,
  NormalRightPosition,
  MaximumRightPosition,
  NormalLeftPosition,
  MaximumLeftPosition
};

//Enum that describe the steer postion
enum NunchuckSpeedEnum {
  MiddleSpeedPosition,
  NormalForwardPosition,
  MaximumForwardPosition,
  NormalBackwardPosition,
  MaximumBackwardPosition
};


//Struct that describe temporar calculation. For debugging purpose
typedef struct{
  double A = 0;
  double B = 0;
  double C = 0;
  NunchuckSteerEnum NunchuckSteer = MiddleSteerPosition;
  NunchuckSpeedEnum NunchuckSpeed = MiddleSpeedPosition;
  bool ASituationHandled = false;
  bool BSituationHandled = false;
} CalculationStruct;

typedef struct{
   uint16_t  start;
   int16_t  steer;
   int16_t  speed;
   uint16_t checksum;
} SerialCommand;
SerialCommand Command;

typedef struct{
   uint16_t start;
   int16_t  cmd1;
   int16_t  cmd2;
   int16_t  speedR;
   int16_t  speedL;
   int16_t  speedR_meas;
   int16_t  speedL_meas;
   int16_t  batVoltage;
   int16_t  boardTemp;
   int16_t  checksum;
} SerialFeedback;
SerialFeedback Feedback;
SerialFeedback NewFeedback;



/* All Values - it would be possible to pass several things around as parameters.
 * But in a limited eviorment this gives the opportuinity to keep a eye on every value at every time.
 * But be careful where to chance the values. That can get messy.
*/
const int SteerEnablePin = 12; //Pin with a switch to check if steering is enabled (railcar mode)
SoftwareSerial SerialBoardOne(10, 11); //D10/D11 - First connector for a Hoverboard ESC - also supply power to the Arduino - don't attach the ESC and a Computer over USB at the same time!
SoftwareSerial SerialBoardTwo(8, 9); //D8/D9 - Secound connector for a Hoverboard ESC
Nunchuk nunchuk; //A4/A5 - Pins of the Nunchuck connector board

//All Informations for the Nuncuck
NunchukStruct NunchukInfo;
//All Informations for the Drive
DriveStruct DriveInfo;
//All Informations that are consistent
ConstStruct ConstInfo;
//All Informations for
CalculationStruct CalculationInfo;
//Steering Enabled Button on the board
boolean SteerEnable = false;

// Global variables
uint8_t idx = 0;                        // Index for new data pointer
uint16_t bufStartFrame;                 // Buffer Start Frame
byte *p;                                // Pointer declaration for the new received data
byte incomingByte;
byte incomingBytePrev;

void setup() {
  pinMode(SteerEnablePin, INPUT);
  Serial.begin(115200);
  //SerialBoardOne.begin(115200);
  //SerialBoardTwo.begin(115200);
  SerialBoardOne.begin(HOVER_SERIAL_BAUD);
  SerialBoardTwo.begin(HOVER_SERIAL_BAUD);
  nunchuk.begin(); //D4/D5

  //Reset all Infos for sure!
  ResetNunchukInfo();
  ResetDriveInfo();
}

void loop()
{
  //Read all the Information you will need.
  SteerEnable = digitalRead(SteerEnablePin);
  ReadNunchukInfo();

  if(NunchukInfo.Connected == true)
  {
    //If drive enable caculate new speeds and stuff.
    GenerateCalculationInfo();
    
     if(NunchukInfo.DeadManSwitchPress == false ||
        CalculationInfo.ASituationHandled == false || 
        CalculationInfo.BSituationHandled == false)
     {
       //Unlogical readings - thats the moment to get to a save state!
       //Don't forget the wrong Calculations just for debug purposesś
       ResetDriveInfo();
     }
     else
     {
      GenerateDriveInfo();
      if(CalculationInfo.ASituationHandled == false || 
         CalculationInfo.BSituationHandled == false)
       {
        ResetDriveInfo();
       }
     }
  }
  else
  {
    //If drive if not enable - disable drive for sure!
    ResetCalculationInfo();
    ResetDriveInfo();
  }
  
  WriteAllInfo();
  
  delay(ConstInfo.Delay);
}

//Read out all Nunchuck Infos if its possible
void ReadNunchukInfo()
{
  //Stay Connected!
  if(!NunchukInfo.Connected)
  {
    NunchukInfo.Connected = nunchuk.connect();
  }
  
  if(NunchukInfo.Connected && nunchuk.update())
  {
    NunchukInfo.Connected = true;
    NunchukInfo.JoyY = nunchuk.joyY();
    NunchukInfo.JoyX = nunchuk.joyX();
    NunchukInfo.ZButton = nunchuk.buttonZ();
    NunchukInfo.CButton = nunchuk.buttonC();
  }
  else
  {
    ResetNunchukInfo();
  }
}

//Resets all Nunhcuk Infos
void ResetNunchukInfo()
{
  NunchukInfo.Connected = false;
  NunchukInfo.JoyY = 127;
  NunchukInfo.JoyX = 127;
  NunchukInfo.ZButton = false;
  NunchukInfo.CButton = false;
  NunchukInfo.DeadManSwitchPress = false; 
}

//Calculate the values we prefer for the drive
void GenerateCalculationInfo()
{
  /*
   * Thanks to the bad Values from the Nunchuck we need to do a lot of work here.
   * Therefor we definie three cicles:
   * - Middle circle - typical area where we want to come to a slow stop
   * - Circle around the Middle - typical the area where we want to keep the speed or slow down faster if we are pointing in another direction
   * - Outer Circle - typical the area where we want to increase the speed or break quite fast if we are pointing in another direction
   * Because of the bad design of a nuncuck we have to find out in witch circle we are.
   * For that we use Pythagoras theorem to determant the distance between the middle and our current position
   * But first as all we have to determant the distance from the middle 
  */
  //Calculate the distance from the middle in the y axle. This will be side a of our Pythagoras theorem. The result can be negativ.
  CalculationInfo.A = ((double)NunchukInfo.JoyY) - ConstInfo.YMiddle;
  //Calculate the distance from the middle in the x axle. This will be side b of our Pythagoras theorem. The result can be negativ.
  CalculationInfo.B = ((double)NunchukInfo.JoyX) - ConstInfo.ZMiddle;
  //Now we are able to calculate side c of our Pythagoras theorem. The result is always positive
  CalculationInfo.C = sqrt(CalculationInfo.A * CalculationInfo.A + CalculationInfo.B * CalculationInfo.B);

  //Reset Positions to determent new one
  CalculationInfo.NunchuckSpeed = MiddleSpeedPosition;
  CalculationInfo.NunchuckSteer = MiddleSteerPosition;
  //Just to keep the code readable - flags that shows that the situation was handled instead of building a tree of conditions. Also do compare them for readablitly.
  CalculationInfo.ASituationHandled = false;
  CalculationInfo.BSituationHandled = false;

  //Don't drive if DeadManSwitch is not pressed!
  if(NunchukInfo.ZButton == false)
  {
    CalculationInfo.NunchuckSpeed = MiddleSpeedPosition;
    CalculationInfo.NunchuckSteer = MiddleSteerPosition;
    CalculationInfo.ASituationHandled = true;
    CalculationInfo.BSituationHandled = true;
    NunchukInfo.DeadManSwitchPress = false;
  }
  if(NunchukInfo.ZButton == true &&
     CalculationInfo.A <=  ConstInfo.MiddleCircle &&
     CalculationInfo.A >=  (ConstInfo.MiddleCircle * -1) &&
     CalculationInfo.B <=  ConstInfo.MiddleCircle &&
     CalculationInfo.B >=  (ConstInfo.MiddleCircle * -1))
  {
    NunchukInfo.DeadManSwitchPress = true;
  }
  
  //Determen Speed
  //We are still in the threshold between Forward and Backwards. Its the size of the middle Circle - that way we can turn on the spot
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.ASituationHandled == false &&
     CalculationInfo.A <=  ConstInfo.MiddleCircle &&
     CalculationInfo.A >=  (ConstInfo.MiddleCircle * -1))
   {
      CalculationInfo.NunchuckSpeed = MiddleSpeedPosition;
      CalculationInfo.ASituationHandled = true;
   }

  //We are beyond the threshold between Forward and Backwards. We are in the inner circle. 
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.ASituationHandled == false &&
     CalculationInfo.A >= 0 &&
     CalculationInfo.C <=  ConstInfo.InnerCircle)
   {
      CalculationInfo.NunchuckSpeed = NormalForwardPosition;
      CalculationInfo.ASituationHandled = true;
   }

  //We are beyond the threshold between Forward and Backwards. We are in the inner circle. 
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.ASituationHandled == false &&
     CalculationInfo.A < 0 &&
     CalculationInfo.C <=  ConstInfo.InnerCircle)
   {
      CalculationInfo.NunchuckSpeed = NormalBackwardPosition;
      CalculationInfo.ASituationHandled = true;
   }

  //We are beyond the InnerCircle.
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.ASituationHandled == false &&
     CalculationInfo.A >= 0 &&
     CalculationInfo.C <=  ConstInfo.EndOfOuterCircle)
   {
      CalculationInfo.NunchuckSpeed = MaximumForwardPosition;
      CalculationInfo.ASituationHandled = true;
   }

  //We are beyond the InnerCircle.
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.ASituationHandled == false &&
     CalculationInfo.A < 0 &&
     CalculationInfo.C <=  ConstInfo.EndOfOuterCircle)
   {
      CalculationInfo.NunchuckSpeed = MaximumBackwardPosition;
      CalculationInfo.ASituationHandled = true;
   }
   
  //Determen steer
  //We are still in the threshold between Forward and Backwards. Its the size of the middle Circle - that way we can turn on the spot
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.BSituationHandled == false &&
     CalculationInfo.B <=  ConstInfo.MiddleCircle &&
     CalculationInfo.B >=  (ConstInfo.MiddleCircle * -1))
   {
      CalculationInfo.NunchuckSteer = MiddleSteerPosition;
      CalculationInfo.BSituationHandled = true;
   }

  //We are beyond the threshold between Forward and Backwards. We are in the inner circle. 
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.BSituationHandled == false &&
     CalculationInfo.B >= 0 &&
     CalculationInfo.C <=  ConstInfo.InnerCircle)
   {
      CalculationInfo.NunchuckSteer = NormalRightPosition;
      CalculationInfo.BSituationHandled = true;
   }

  //We are beyond the threshold between Forward and Backwards. We are in the inner circle. 
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.BSituationHandled == false &&
     CalculationInfo.B < 0 &&
     CalculationInfo.C <=  ConstInfo.InnerCircle)
   {
      CalculationInfo.NunchuckSteer = NormalLeftPosition;
      CalculationInfo.BSituationHandled = true;
   }

  //We are beyond the InnerCircle.
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.BSituationHandled == false &&
     CalculationInfo.B >= 0 &&
     CalculationInfo.C <=  ConstInfo.EndOfOuterCircle)
   {
      CalculationInfo.NunchuckSteer = MaximumRightPosition;
      CalculationInfo.BSituationHandled = true;
   }

  //We are beyond the InnerCircle.
  if(NunchukInfo.DeadManSwitchPress == true &&
     CalculationInfo.BSituationHandled == false &&
     CalculationInfo.B < 0 &&
     CalculationInfo.C <=  ConstInfo.EndOfOuterCircle)
   {
      CalculationInfo.NunchuckSteer = MaximumLeftPosition;
      CalculationInfo.BSituationHandled = true;
   }
}

void GenerateDriveInfo()
{
  GenerateDriveSpeed();
  GenerateDriveSteer();
}

void GenerateDriveSpeed()
{
  
  //Just to keep the code readable - flags that shows that the situation was handled instead of building a tree of conditions. Also do compare them for readablitly.
  DriveInfo.SpeedHandled = false;

  //Forward
  //Increase Speed to the Max
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed >= ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == MaximumForwardPosition &&
    (DriveInfo.DrivingDirection == ForwardDriving ||
    DriveInfo.DrivingDirection == NoDriving))
  {
    if((DriveInfo.Speed + ConstInfo.SpeedIncreasement) > ConstInfo.SpeedOverallMax)
    {
      DriveInfo.Speed = ConstInfo.SpeedOverallMax;
    }
    else
    {
      DriveInfo.Speed += ConstInfo.SpeedIncreasement;
    }
    DriveInfo.DrivingDirection = ForwardDriving;
    DriveInfo.SpeedHandled = true;
  }

  //Keep Speed
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed > ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == NormalForwardPosition &&
    (DriveInfo.DrivingDirection == ForwardDriving ||
    DriveInfo.DrivingDirection == NoDriving))
  {
    DriveInfo.SpeedHandled = true;
  }

  //decrease speed normal
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed > ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == MiddleSpeedPosition &&
    (DriveInfo.DrivingDirection == ForwardDriving ||
    DriveInfo.DrivingDirection == NoDriving))
  {
    if((DriveInfo.Speed - ConstInfo.SpeedDecreasementNormal) < ConstInfo.SpeedOverallZero)
    {
      DriveInfo.Speed = ConstInfo.SpeedOverallZero;
      DriveInfo.DrivingDirection = NoDriving;
    }
    else
    {
      DriveInfo.Speed -= ConstInfo.SpeedDecreasementNormal;
    }
    DriveInfo.SpeedHandled = true;
  }
  
  //decrease speed fast
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed > ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == NormalBackwardPosition &&
    DriveInfo.DrivingDirection == ForwardDriving)
  {
    if((DriveInfo.Speed - ConstInfo.SpeedDecreasementFast) < ConstInfo.SpeedOverallZero)
    {
      DriveInfo.Speed = ConstInfo.SpeedOverallZero;
    }
    else
    {
      DriveInfo.Speed -= ConstInfo.SpeedDecreasementFast;
    }
    DriveInfo.SpeedHandled = true;
  }

  //decrease speed fast
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed > ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == MaximumBackwardPosition &&
    DriveInfo.DrivingDirection == ForwardDriving)
  {
    if((DriveInfo.Speed - ConstInfo.SpeedDecreasementMax) < ConstInfo.SpeedOverallZero)
    {
      DriveInfo.Speed = ConstInfo.SpeedOverallZero;
    }
    else
    {
      DriveInfo.Speed -= ConstInfo.SpeedDecreasementMax;
    }
    DriveInfo.SpeedHandled = true;
  }
  
  //Backward
  //Increase Speed to the Max
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed <= ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == MaximumBackwardPosition &&
    (DriveInfo.DrivingDirection == BackwardDriving ||
    DriveInfo.DrivingDirection == NoDriving))
  {
    if((DriveInfo.Speed - ConstInfo.SpeedIncreasement) < ConstInfo.SpeedOverallMin)
    {
      DriveInfo.Speed = ConstInfo.SpeedOverallMin;
    }
    else
    {
      DriveInfo.Speed -= ConstInfo.SpeedIncreasement;
    }
    DriveInfo.DrivingDirection = BackwardDriving;
    DriveInfo.SpeedHandled = true;
  }

  //Keep Speed
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed <= ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == NormalBackwardPosition)
  {
    DriveInfo.SpeedHandled = true;
  }

  //decrease speed normal
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed <= ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == MiddleSpeedPosition)
  {
    if((DriveInfo.Speed + ConstInfo.SpeedDecreasementNormal) > ConstInfo.SpeedOverallZero)
    {
      DriveInfo.Speed = ConstInfo.SpeedOverallZero;
      DriveInfo.DrivingDirection = NoDriving;
    }
    else
    {
      DriveInfo.Speed += ConstInfo.SpeedDecreasementNormal;
    }
    DriveInfo.SpeedHandled = true;
  }

  //decrease speed fast
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed <= ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == NormalForwardPosition)
  {
    if((DriveInfo.Speed + ConstInfo.SpeedDecreasementFast) > ConstInfo.SpeedOverallZero)
    {
      DriveInfo.Speed = ConstInfo.SpeedOverallZero;
    }
    else
    {
      DriveInfo.Speed += ConstInfo.SpeedDecreasementFast;
    }
    DriveInfo.SpeedHandled = true;
  }

  //decrease speed fast
  if(DriveInfo.SpeedHandled == false &&
    DriveInfo.Speed <= ConstInfo.SpeedOverallZero &&
    CalculationInfo.NunchuckSpeed == MaximumForwardPosition)
  {
    if((DriveInfo.Speed + ConstInfo.SpeedDecreasementMax) > ConstInfo.SpeedOverallZero)
    {
      DriveInfo.Speed = ConstInfo.SpeedOverallZero;
    }
    else
    {
      DriveInfo.Speed += ConstInfo.SpeedDecreasementMax;
    }
    DriveInfo.SpeedHandled = true;
  }
}
void GenerateDriveSteer()
{
  
  //Just to keep the code readable - flags that shows that the situation was handled instead of building a tree of conditions. Also do compare them for readablitly.
  DriveInfo.SteerHandled = false;
  if(SteerEnable)
  {
    //Right
    //Increase Steer to the Max
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer >= ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == MaximumRightPosition)
    {
      if((DriveInfo.Steer + ConstInfo.SteerIncreasement) > ConstInfo.SteerOverallRight)
      {
        DriveInfo.Steer = ConstInfo.SteerOverallRight;
      }
      else
      {
        DriveInfo.Steer += ConstInfo.SteerIncreasement;
      }
      DriveInfo.SteerHandled = true;
    }
  
    //Keep Steer
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer > ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == NormalRightPosition)
    {
      DriveInfo.SteerHandled = true;
    }
  
    //decrease Steer normal
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer > ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == MiddleSteerPosition)
    {
      if((DriveInfo.Steer - ConstInfo.SteerDecreasementNormal) < ConstInfo.SteerOverallZero)
      {
        DriveInfo.Steer = ConstInfo.SteerOverallZero;
      }
      else
      {
        DriveInfo.Steer -= ConstInfo.SteerDecreasementNormal;
      }
      DriveInfo.SteerHandled = true;
    }
    
    //decrease Steer fast
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer > ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == NormalLeftPosition)
    {
      if((DriveInfo.Steer - ConstInfo.SteerDecreasementFast) < ConstInfo.SteerOverallZero)
      {
        DriveInfo.Steer = ConstInfo.SteerOverallZero;
      }
      else
      {
        DriveInfo.Steer -= ConstInfo.SteerDecreasementFast;
      }
      DriveInfo.SteerHandled = true;
    }
  
    //decrease Steer fast
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer > ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == MaximumLeftPosition)
    {
      if((DriveInfo.Steer - ConstInfo.SteerDecreasementMax) < ConstInfo.SteerOverallZero)
      {
        DriveInfo.Steer = ConstInfo.SteerOverallZero;
      }
      else
      {
        DriveInfo.Steer -= ConstInfo.SteerDecreasementMax;
      }
      DriveInfo.SteerHandled = true;
    }
    
    //Left
    //Increase Steer to the Max
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer <= ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == MaximumLeftPosition)
    {
      if((DriveInfo.Steer - ConstInfo.SteerIncreasement) < ConstInfo.SteerOverallLeft)
      {
        DriveInfo.Steer = ConstInfo.SteerOverallLeft;
      }
      else
      {
        DriveInfo.Steer -= ConstInfo.SteerIncreasement;
      }
      DriveInfo.SteerHandled = true;
    }
  
    //Keep Steer
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer <= ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == NormalLeftPosition)
    {
      DriveInfo.SteerHandled = true;
    }
  
    //decrease Steer normal
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer <= ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == MiddleSteerPosition)
    {
      if((DriveInfo.Steer + ConstInfo.SteerDecreasementNormal) > ConstInfo.SteerOverallZero)
      {
        DriveInfo.Steer = ConstInfo.SteerOverallZero;
      }
      else
      {
        DriveInfo.Steer += ConstInfo.SteerDecreasementNormal;
      }
      DriveInfo.SteerHandled = true;
    }
  
    //decrease Steer fast
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer <= ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == NormalRightPosition)
    {
      if((DriveInfo.Steer + ConstInfo.SteerDecreasementFast) > ConstInfo.SteerOverallZero)
      {
        DriveInfo.Steer = ConstInfo.SteerOverallZero;
      }
      else
      {
        DriveInfo.Steer += ConstInfo.SteerDecreasementFast;
      }
      DriveInfo.SteerHandled = true;
    }
  
    //decrease Steer fast
    if(DriveInfo.SteerHandled == false &&
      DriveInfo.Steer <= ConstInfo.SteerOverallZero &&
      CalculationInfo.NunchuckSteer == MaximumRightPosition)
    {
      if((DriveInfo.Steer + ConstInfo.SteerDecreasementMax) > ConstInfo.SteerOverallZero)
      {
        DriveInfo.Steer = ConstInfo.SteerOverallZero;
      }
      else
      {
        DriveInfo.Steer += ConstInfo.SteerDecreasementMax;
      }
      DriveInfo.SteerHandled = true;
    }
  }
  else
  {
    DriveInfo.Steer = 0;
    DriveInfo.SteerHandled = true;
  }
}


void ResetDriveInfo()
{
  DriveInfo.Speed = 0;
  DriveInfo.Steer = 0;
  DriveInfo.SpeedHandled = false;
  DriveInfo.SteerHandled = false;
}

void ResetCalculationInfo()
{
  CalculationInfo.A = 0.0;
  CalculationInfo.B = 0.0;
  CalculationInfo.C = 0.0;
  CalculationInfo.NunchuckSteer = MiddleSteerPosition;
  CalculationInfo.NunchuckSpeed = MiddleSpeedPosition;
  CalculationInfo.ASituationHandled = false;
  CalculationInfo.BSituationHandled = false;
}

void WriteAllInfo()
{
  Serial.print(1000);
  Serial.print(" ");
  Serial.print(-1000);
  Serial.print(" ");
  Serial.print(SteerEnable);
  Serial.print(" ");
  Serial.print(NunchukInfo.Connected);
  Serial.print(" ");
  Serial.print(NunchukInfo.JoyY);
  Serial.print(" ");
  Serial.print(NunchukInfo.JoyX);
  Serial.print(" ");
  Serial.print(NunchukInfo.ZButton);
  Serial.print(" ");
  Serial.print(NunchukInfo.CButton);
  Serial.print(" ");
  Serial.print(NunchukInfo.DeadManSwitchPress);
  Serial.print(" ");
  Serial.print(CalculationInfo.A);
  Serial.print(" ");
  Serial.print(CalculationInfo.B);
  Serial.print(" ");
  Serial.print(CalculationInfo.C);
  Serial.print(" ");  
  Serial.print(CalculationInfo.NunchuckSteer);
  Serial.print(" ");
  Serial.print(CalculationInfo.NunchuckSpeed);
  Serial.print(" ");
  Serial.print(CalculationInfo.ASituationHandled);
  Serial.print(" ");
  Serial.print(CalculationInfo.BSituationHandled);
  Serial.print(" ");
  Serial.print(DriveInfo.Speed);
  Serial.print(" ");
  Serial.print(DriveInfo.Steer);
  Serial.print(" ");
  Serial.print(DriveInfo.SpeedHandled);
  Serial.print(" ");
  Serial.print(DriveInfo.SteerHandled);
  Serial.print(" ");
  Serial.print(DriveInfo.DrivingDirection);
  Serial.println();
  
  //SerialBoardOne.write ((uint8_t *) &DriveInfo.Steer, sizeof(DriveInfo.Steer));
  //SerialBoardOne.write((uint8_t *) &DriveInfo.Speed, sizeof(DriveInfo.Speed));
  //SerialBoardTwo.write ((uint8_t *) &DriveInfo.Steer, sizeof(DriveInfo.Steer));
  //SerialBoardTwo.write((uint8_t *) &DriveInfo.Speed, sizeof(DriveInfo.Speed));
  Send(DriveInfo.Steer, DriveInfo.Speed);
}

// ########################## SEND ##########################
void Send(int16_t uSteer, int16_t uSpeed)
{
  // Create command
  Command.start    = (uint16_t)START_FRAME;
  Command.steer    = (int16_t)uSteer;
  Command.speed    = (int16_t)uSpeed;
  Command.checksum = (uint16_t)(Command.start ^ Command.steer ^ Command.speed);

  // Write to Serial
  SerialBoardOne.write((uint8_t *) &Command, sizeof(Command)); 
  SerialBoardTwo.write((uint8_t *) &Command, sizeof(Command)); 
}
