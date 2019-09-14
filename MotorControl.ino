#include <SoftwareSerial.h>
#include <NintendoExtensionCtrl.h>

const float SpeedOverallMax = 1000.0;
const float SpeedOverallZero = 0.0;
const float SpeedOverallMin = -1000.0;
const float SpeedIncreasement = 6.0;
const float SpeedDecreasementNormal = 6.0;
const float SpeedDecreasementFast = 15.0;
const float SpeedDecreasementMax = 30.0;

const int ReverseMax = 33;
const int SpeedZeroPosMin = 130;
const int SpeedZeroPosMax = 137;
const int ForwardMax = 232;

const float SteerOverallRight = 1000.0;
const float SteerOverallZero = 0.0;
const float SteerOverallLeft = -1000.0;
const float SteerIncreasement = 15.0;
const float SteerDecreasementNormal = 15.0;
const float SteerDecreasementFast = 15.0;
const float SteerDecreasementMax = 15.0;

const int LeftMax = 20;
const int SteerZeroPosMin = 120;
const int SteerZeroPosMax = 125;
const int RigthMax = 219;

const int SteerEnablePin = 12;

typedef struct MsgToHoverboard_t{
  unsigned char SOM;  // Start of Message
  unsigned char CI;   // continuity counter
  unsigned char len;  // len is len of bytes to follow, NOT including CS
  unsigned char cmd;  // read or write
  unsigned char code; // code of value to write
  int32_t pwm1;           // absolute value ranging from -1000 to 1000 .. Duty Cycle *10 for first wheel
  int32_t pwm2;           // absolute value ranging from -1000 to 1000 .. Duty Cycle *10 for second wheel
  unsigned char CS;   // checksumm
};

typedef union UART_Packet_t{
  MsgToHoverboard_t msgToHover;
  byte UART_Packet[sizeof(MsgToHoverboard_t)];
};

Nunchuk nunchuk; //A4/A5
SoftwareSerial SerialBoardOne(10, 11); //D10/D11
SoftwareSerial SerialBoardTwo(8, 9); //D8/D9

void setup() {
  pinMode(SteerEnablePin, INPUT);
  Serial.begin(115200);
  SerialBoardOne.begin(115200);
  SerialBoardTwo.begin(115200);
  nunchuk.begin(); //D4/D5
}


boolean readSuccess = false;
boolean firstConnection = true;
boolean speedZeroPos = true;
int currentSpeed = SpeedOverallZero;
int currentSteer = SteerOverallZero;
char hoverboardCI = 0;  // Global variable which tracks CI
void loop()
{
  UART_Packet_t ups;
  boolean steerEnable = true;
  
  steerEnable = digitalRead(SteerEnablePin);
  
  //Wurde bei der letzten Runde die Nunchuck noch nicht Verbunden?
  if(!readSuccess)
  {
    //Dann verbinde
    readSuccess = nunchuk.connect(); //Connect controller
    firstConnection = true;
  }
  
  if(readSuccess)
  {
    //Wenn Nunchuck verbunden ist versuche Daten zu lesen
    readSuccess = nunchuk.update(); // Get new data from the controller
    
    if(readSuccess)
    {
      //Wenn Daten Auslesen funktioniert hat, lese die Werte ein
      int joyY = 127; //Vor/Zurück
      int joyX = 127; //Links/Rechts
      boolean zButton = false; //Tot Mann Schalter

      joyY = nunchuk.joyY();
      joyX = nunchuk.joyX();
      zButton = nunchuk.buttonZ();
      
      //Wenn wir im ersten Connect sind (firstConnection == true) muss Joystick zentiert sein und Totmannschalter gedrückt)
      if(firstConnection &&
        zButton &&
        joyY >= SpeedZeroPosMin &&
        joyY <= SpeedZeroPosMax &&
        joyX >= SteerZeroPosMin &&
        joyX <= SteerZeroPosMax)
      {
        //Todmann Schalter gedrück, Joystick in der mitte - ab jetzt darf gesteuert werden
        firstConnection = false;
      }

      //Nur wenn gesteuert werden darf
      if(!firstConnection &&
        zButton)
      {
        //Lenken (Wenn aktiv)
        if(steerEnable)
        {
          //Soll langsam wieder gerade gesteuert werden? (Joystick mittig)
          if(joyX >= SteerZeroPosMin &&
            joyX <= SteerZeroPosMax)
          {
            //Fahren wir nach Rechts?
            if(currentSteer > SteerOverallZero)
            {
              //Fahren wir schon gerade aus?
              if(currentSteer - SteerDecreasementNormal <= SteerOverallZero)
              {
                currentSteer = SteerOverallZero;
              }
              else
              {
                currentSteer -= SteerDecreasementNormal;
              }
            }
            else
            {
              //Fahren Rückwärts
              //stehen wir schon?
              if(currentSteer + SteerDecreasementNormal > SteerOverallZero)
              {
                currentSteer = SteerOverallZero;
              }
              else
              {
                currentSteer += SteerDecreasementNormal;
              }
            }
          }
          else
          {
            //Ist der Joystick leicht nach Links gedrückt und wir fahren nach rechts
            if(joyX > SteerZeroPosMax &&
              joyX < RigthMax &&
              currentSteer < SteerOverallZero)
            {
              //Minimum schon erreicht?
              currentSteer += SteerDecreasementFast;
            }
            else
            {
              //Ist der Joystick leicht nach hinten gedrückt und wir fahren vorwärts
              if(joyX < SteerZeroPosMin &&
                joyX > LeftMax &&
                currentSteer > SteerOverallZero)
              {
                //Minimum schon erreicht?
                currentSteer -= SteerDecreasementFast;
              }
              else
              {
                //Ist Joystick ganz nach rechts gedrück?
                if(joyX >= RigthMax)
                {
                  //Fahren wir nacht rechts?
                  if(currentSteer >= SteerOverallZero)
                  {
                    //Maximum nach rechts schon erreicht?
                    if(currentSteer + SteerIncreasement > SteerOverallRight)
                    {
                      currentSteer = SteerOverallRight;
                    }
                    else
                    {
                      currentSteer += SteerIncreasement;
                    }
                  }
                  else
                  {
                    //Wenn wir nach Links fahren
                    //Mitte schon erreicht?
                    currentSteer += SteerDecreasementMax;
                  }
                }
                else
                {
                  //Ist Joystick ganz nach Links gedrück?
                  if(joyX <= LeftMax)
                  {
                    //Fahren wir Rechts?
                    if(currentSteer <= SteerOverallZero)
                    {
                      //Minimum von links schon erreicht?
                      if(currentSteer - SteerIncreasement < SteerOverallLeft)
                      {
                        currentSteer = SteerOverallLeft;
                      }
                      else
                      {
                        currentSteer -= SteerIncreasement;
                      }
                    }
                    else
                    {
                      //Wir fahren nach Links
                      //Stop schon erreich?
                      currentSteer -= SteerDecreasementMax;
                    }
                  }
                }
              }
            }
          }
        }
        else
        {
          currentSteer = SteerOverallZero;
        }
        
        //Geschwindigkeit (nur wenn einmal die Y-Achse-Mitte erreicht wurde)
        if(speedZeroPos &&
          joyY >= SpeedZeroPosMin &&
          joyY <= SpeedZeroPosMax)
        {
          //Todmann Schalter gedrück, Joystick in der Y-Achse-Mitte - ab jetzt darf gesteuert werden
          speedZeroPos = false;
        }
        
        if(!speedZeroPos)
        {
          //Soll langsam gebremst werden? (Joystick mittig)
          if(joyY >= SpeedZeroPosMin &&
            joyY <= SpeedZeroPosMax)
          {
            //Fahren wir vorwärts?
            if(currentSpeed > SpeedOverallZero)
            {
              //stehen wir schon?
              if(currentSpeed - SpeedDecreasementNormal <= SpeedOverallZero)
              {
                currentSpeed = SpeedOverallZero;
              }
              else
              {
                currentSpeed -= SpeedDecreasementNormal;
              }
            }
            else
            {
              //Fahren Rückwärts
              //stehen wir schon?
              if(currentSpeed + SpeedDecreasementNormal > SpeedOverallZero)
              {
                currentSpeed = SpeedOverallZero;
              }
              else
              {
                currentSpeed += SpeedDecreasementNormal;
              }
            }
          }
          else
          {
            //Ist der Joystick leicht nach vorne gedrückt und wir fahren Rückwärts
            if(joyY > SpeedZeroPosMax &&
              joyY < ForwardMax &&
              currentSpeed < SpeedOverallZero)
            {
              //Minimum schon erreicht?
              if(currentSpeed + SpeedDecreasementFast >= SpeedOverallZero)
              {
                currentSpeed = SpeedOverallZero;
                speedZeroPos = true;
              }
              else
              {
                currentSpeed += SpeedDecreasementFast;
              }
            }
            else
            {
              //Ist der Joystick leicht nach hinten gedrückt und wir fahren vorwärts
              if(joyY < SpeedZeroPosMin &&
                joyY > ReverseMax &&
                currentSpeed > SpeedOverallZero)
              {
                //Minimum schon erreicht?
                if(currentSpeed - SpeedDecreasementFast <= SpeedOverallZero)
                {
                  currentSpeed = SpeedOverallZero;
                  speedZeroPos = true;
                }
                else
                {
                  currentSpeed -= SpeedDecreasementFast;
                }
              }
              else
              {
                //Ist Joystick ganz nach vorne gedrück?
                if(joyY >= ForwardMax)
                {
                  //Fahren wir vorwärts?
                  if(currentSpeed >= SpeedOverallZero)
                  {
                    //Maximum schon erreicht?
                    if(currentSpeed + SpeedIncreasement > SpeedOverallMax)
                    {
                      currentSpeed = SpeedOverallMax;
                    }
                    else
                    {
                      currentSpeed += SpeedIncreasement;
                    }
                  }
                  else
                  {
                    //Wenn wir rückswärts fahren
                    //Stop schon erreicht?
                    if(currentSpeed + SpeedDecreasementMax >= SpeedOverallZero)
                    {
                      currentSpeed = SpeedOverallZero;
                      speedZeroPos = true;
                    }
                    else
                    {
                      currentSpeed += SpeedDecreasementMax;
                    }
                  }
                }
                else
                {
                  //Ist Joystick ganz nach hinten gedrück?
                  if(joyY <= ReverseMax)
                  {
                    //Fahren wir rückwärts?
                    if(currentSpeed <= SpeedOverallZero)
                    {
                      //Minimum schon erreicht?
                      if(currentSpeed - SpeedIncreasement < SpeedOverallMin)
                      {
                        currentSpeed = SpeedOverallMin;
                      }
                      else
                      {
                        currentSpeed -= SpeedIncreasement;
                      }
                    }
                    else
                    {
                      //Wir fahren vorwärts
                      //Stop schon erreich?
                      if(currentSpeed - SpeedDecreasementMax <= SpeedOverallZero)
                      {
                        currentSpeed = SpeedOverallZero;
                        speedZeroPos = true;
                      }
                      else
                      {
                        currentSpeed -= SpeedDecreasementMax;
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      else
      { //(!firstConnection && zButton) == false
        //Totmannschalter nicht gedrückt: Bremsen!
        currentSpeed = SpeedOverallZero;
        currentSteer = SteerOverallZero;
        firstConnection = true;
      }
    }
    else
    { //readSuccess == false
      //Controller ab, irgendwas stimmt nicht: Bremsen!
      currentSpeed = SpeedOverallZero;
      currentSteer = SteerOverallZero;
      firstConnection = true;
    }
  }

  ups.msgToHover.SOM = 4 ;    // Start of Message, 4 for No ACKs;
  ups.msgToHover.CI = ++hoverboardCI; // Message Continuity Indicator. Subsequent Messages with the same CI are discarded, need to be incremented.
  ups.msgToHover.len = 1 + 1 + 4 + 4 ; // cmd(1), code(1), pwm1(4) and pwm2(4)
  ups.msgToHover.cmd  = 'r';  // Pretend to send answer to read request. This way HB will not reply. Change to 'W' to get confirmation from board
  ups.msgToHover.code = 0x0E; // "simpler PWM"
  ups.msgToHover.pwm1 = int(currentSpeed);
  ups.msgToHover.pwm2 = int(currentSpeed);
  ups.msgToHover.CS = 0;
  
  for (int i = 0; i < (2 + ups.msgToHover.len); i++){  // Calculate checksum. 2 more for CI and len.
    ups.msgToHover.CS -= ups.UART_Packet[i+1];
  }
  
  SerialBoardOne.write(ups.UART_Packet, sizeof(UART_Packet_t));
  SerialBoardTwo.write(ups.UART_Packet, sizeof(UART_Packet_t));
  
  /*if (SerialBoardOne.available())
  {
    
    Serial.println("SerialBoardOne:");
    Serial.println(">>");
    Serial.write(SerialBoardOne.read());
    Serial.println("<<");
  }
  if (SerialBoardTwo.available())
  {
    
    Serial.println("SerialBoardOne:");
    Serial.println(">>");
    Serial.write(SerialBoardTwo.read());
    Serial.println("<<");
  }*/
  
  Serial.print(SpeedOverallMax);
  Serial.print(" ");
  Serial.print(SpeedOverallMin);
  Serial.print(" ");
  Serial.print(currentSpeed);
  Serial.print(" ");
  Serial.print(currentSteer);
  Serial.println();
  delay(10);
}


