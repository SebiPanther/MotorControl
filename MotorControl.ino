#include <SoftwareSerial.h>
#include <NintendoExtensionCtrl.h>

const int SpeedOverallMax = 1000;
const int SpeedOverallZero = 0;
const int SpeedOverallMin = -1000;
const int SpeedIncreasement = 2;
const int SpeedDecreasementNormal = 2;
const int SpeedDecreasementFast = 5;
const int SpeedDecreasementMax = 10;

const int ReverseMax = 0;
const int SpeedZeroPosMin = 126;
const int SpeedZeroPosMax = 130;
const int ForwardMax = 255;

const int SteerOverallRight = 250;
const int SteerOverallZero = 0;
const int SteerOverallLeft = -250;
const int SteerIncreasement = 1;
const int SteerDecreasementNormal = 2;
const int SteerDecreasementFast = 5;
const int SteerDecreasementMax = 10;

const int LeftMax = 0;
const int SteerZeroPosMin = 126;
const int SteerZeroPosMax = 130;
const int RigthMax = 255;

const int SteerEnablePin = 12;

typedef struct MsgToHoverboard_t{
  unsigned char SOM;  // 0x02
  unsigned char len;  // len is len of ALL bytes to follow, including CS
  unsigned char cmd;  // 'W'
  unsigned char code; // code of value to write
  int16_t base_pwm;   // absolute value ranging from -1000 to 1000 .. base_pwm plus/minus steer is the raw PWM value
  int16_t steer;      // absolute value ranging from -1000 to 1000 .. wether steer is added or substracted depends on the side R/L
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
int currentSpeed = SpeedOverallZero;
int currentSteer = SteerOverallZero;
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
          //Geschwindigkeit
          //Soll langsam gebremst werden? (Joystick mittig)
          if(joyX >= SteerZeroPosMin &&
            joyX <= SteerZeroPosMax)
          {
            //Fahren wir vorwärts?
            if(currentSteer > SteerOverallZero)
            {
              //stehen wir schon?
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
            //Ist der Joystick leicht nach vorne gedrückt und wir fahren Rückwärts
            if(joyX > SteerZeroPosMax &&
              joyX < RigthMax &&
              currentSteer < SteerOverallZero)
            {
              //Minimum schon erreicht?
              if(currentSteer + SteerDecreasementFast >= SteerOverallZero)
              {
                currentSteer = SteerOverallZero;
                firstConnection = true;
              }
              else
              {
                currentSteer += SteerDecreasementFast;
              }
            }
            else
            {
              //Ist der Joystick leicht nach hinten gedrückt und wir fahren vorwärts
              if(joyX < SteerZeroPosMin &&
                joyX > LeftMax &&
                currentSteer > SteerOverallZero)
              {
                //Minimum schon erreicht?
                if(currentSteer - SteerDecreasementFast <= SteerOverallZero)
                {
                  currentSteer = SteerOverallZero;
                  firstConnection = true;
                }
                else
                {
                  currentSteer -= SteerDecreasementFast;
                }
              }
              else
              {
                //Ist Joystick ganz nach vorne gedrück?
                if(joyX >= RigthMax)
                {
                  //Fahren wir vorwärts?
                  if(currentSteer >= SteerOverallZero)
                  {
                    //Maximum schon erreicht?
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
                    //Wenn wir rückswärts fahren
                    //Stop schon erreicht?
                    if(currentSteer + SteerDecreasementMax >= SteerOverallZero)
                    {
                      currentSteer = SteerOverallZero;
                      firstConnection = true;
                    }
                    else
                    {
                      currentSteer += SteerDecreasementMax;
                    }
                  }
                }
                else
                {
                  //Ist Joystick ganz nach hinten gedrück?
                  if(joyX <= LeftMax)
                  {
                    //Fahren wir rückwärts?
                    if(currentSteer <= SteerOverallZero)
                    {
                      //Minimum schon erreicht?
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
                      //Wir fahren vorwärts
                      //Stop schon erreich?
                      if(currentSteer - SteerDecreasementMax <= SteerOverallZero)
                      {
                        currentSteer = SteerOverallZero;
                        firstConnection = true;
                      }
                      else
                      {
                        currentSteer -= SteerDecreasementMax;
                      }
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
        
        //Geschwindigkeit
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
              firstConnection = true;
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
                firstConnection = true;
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
                    firstConnection = true;
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
                      firstConnection = true;
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
      else
      {
        //Totmannschalter nicht gedrückt, Controller ab, irgendwas stimmt nicht: Bremsen!
        currentSpeed = SpeedOverallZero;
        currentSteer = SteerOverallZero;
      }
    }
    else
    {
      //Totmannschalter nicht gedrückt, Controller ab, irgendwas stimmt nicht: Bremsen!
      currentSpeed = SpeedOverallZero;
      currentSteer = SteerOverallZero;
    }
  }
  
  ups.msgToHover.SOM = 2 ;  // PROTOCOL_SOM; //Start of Message;
  ups.msgToHover.len = 7;   // payload + SC only
  ups.msgToHover.cmd  = 'W'; // PROTOCOL_CMD_WRITEVAL;  // Write value
  ups.msgToHover.code = 0x07; // speed data from params array
  ups.msgToHover.base_pwm = currentSpeed;
  ups.msgToHover.steer = currentSteer;
  ups.msgToHover.CS = 0;

  for (int i = 0; i < ups.msgToHover.len; i++){
    ups.msgToHover.CS -= ups.UART_Packet[i+1];
  }
  
  SerialBoardOne.write(ups.UART_Packet, sizeof(UART_Packet_t));
  SerialBoardTwo.write(ups.UART_Packet, sizeof(UART_Packet_t));
  /*
  if (SerialBoardOne.available())
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
