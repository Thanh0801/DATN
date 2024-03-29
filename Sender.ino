#include <SoftwareSerial.h>
#include "E32-TTL-100.h"
#include <TinyGPS++.h>

#define M0_PIN  6
#define M1_PIN  5
#define AUX_PIN A0
#define SOFT_RX 3
#define SOFT_TX 4
#define VOLTAGE_PIN A1

TinyGPSPlus gps;

SoftwareSerial softSerial(SOFT_RX, SOFT_TX);  // LORA RX, LORA TX

//=== AUX ===========================================+
bool AUX_HL;
bool ReadAUX()
{
  int val = analogRead(AUX_PIN);

  if(val<50)
  {
    AUX_HL = LOW;
  }else {
    AUX_HL = HIGH;
  }

  return AUX_HL;
}

//return default status
RET_STATUS WaitAUX_H()
{
  RET_STATUS STATUS = RET_SUCCESS;

  uint8_t cnt = 0;
  uint8_t data_buf[100], data_len;

  while((ReadAUX()==LOW) && (cnt++<TIME_OUT_CNT))
  {
    Serial.print(".");
    delay(100);
  }

  if(cnt==0)
  {
  }
  else if(cnt>=TIME_OUT_CNT)
  {
    STATUS = RET_TIMEOUT;
    Serial.println(" TimeOut");
  }
  else
  {
    Serial.println("");
  }

  return STATUS;
}
//=== AUX ===========================================-
//=== Mode Select ===================================+
bool chkModeSame(MODE_TYPE mode)
{
  static MODE_TYPE pre_mode = MODE_INIT;

  if(pre_mode == mode)
  {
    //Serial.print("SwitchMode: (no need to switch) ");  Serial.println(mode, HEX);
    return true;
  }
  else
  {
    Serial.print("SwitchMode: from ");  Serial.print(pre_mode, HEX);  Serial.print(" to ");  Serial.println(mode, HEX);
    pre_mode = mode;
    return false;
  }
}

void SwitchMode(MODE_TYPE mode)
{
  if(!chkModeSame(mode))
  {
    WaitAUX_H();

    switch (mode)
    {
      case MODE_0_NORMAL:
        // Mode 0 | normal operation
        digitalWrite(M0_PIN, LOW);
        digitalWrite(M1_PIN, LOW);
        break;
      case MODE_1_WAKE_UP:
        digitalWrite(M0_PIN, HIGH);
        digitalWrite(M1_PIN, LOW);
        break;
      case MODE_2_POWER_SAVIN:
        digitalWrite(M0_PIN, LOW);
        digitalWrite(M1_PIN, HIGH);
        break;
      case MODE_3_SLEEP:
        // Mode 3 | Setting operation
        digitalWrite(M0_PIN, HIGH);
        digitalWrite(M1_PIN, HIGH);
        break;
      default:
        return ;
    }

    WaitAUX_H();
    delay(10);
  }
}
//=== Mode Select ===================================-
//=== Basic cmd =====================================+
void cleanUARTBuf()
{
  bool IsNull = true;

  while (softSerial.available())
  {
    IsNull = false;

    softSerial.read();
  }
}

void triple_cmd(SLEEP_MODE_CMD_TYPE Tcmd)
{
  uint8_t CMD[3] = {Tcmd, Tcmd, Tcmd};
  softSerial.write(CMD, 3);
  delay(50);  //need ti check
}

RET_STATUS Module_info(uint8_t* pReadbuf, uint8_t buf_len)
{
  RET_STATUS STATUS = RET_SUCCESS;
  uint8_t Readcnt, idx;

  Readcnt = softSerial.available();
  //Serial.print("softSerial.available(): ");  Serial.print(Readcnt);  Serial.println(" bytes.");
  if (Readcnt == buf_len)
  {
    for(idx=0;idx<buf_len;idx++)
    {
      *(pReadbuf+idx) = softSerial.read();
      Serial.print(" 0x");
      Serial.print(0xFF & *(pReadbuf+idx), HEX);    // print as an ASCII-encoded hexadecimal
    } Serial.println("");
  }
  else
  {
    STATUS = RET_DATA_SIZE_NOT_MATCH;
    Serial.print("  RET_DATA_SIZE_NOT_MATCH - Readcnt: ");  Serial.println(Readcnt);
    cleanUARTBuf();
  }

  return STATUS;
}
//=== Basic cmd =====================================-
//=== Sleep mode cmd ================================+
RET_STATUS Write_CFG_PDS(struct CFGstruct* pCFG)
{
  softSerial.write((uint8_t *)pCFG, 6);

  WaitAUX_H();
  delay(1200);  //need to check

  return RET_SUCCESS;
}

RET_STATUS Read_CFG(struct CFGstruct* pCFG)
{
  RET_STATUS STATUS = RET_SUCCESS;

  //1. read UART buffer.
  cleanUARTBuf();

  //2. send CMD
  triple_cmd(R_CFG);

  //3. Receive configure
  STATUS = Module_info((uint8_t *)pCFG, sizeof(CFGstruct));
  if(STATUS == RET_SUCCESS)
  {
    Serial.print("  HEAD:     ");  Serial.println(pCFG->HEAD, HEX);
    Serial.print("  ADDH:     ");  Serial.println(pCFG->ADDH, HEX);
    Serial.print("  ADDL:     ");  Serial.println(pCFG->ADDL, HEX);

    Serial.print("  CHAN:     ");  Serial.println(pCFG->CHAN, HEX);
  }

  return STATUS;
}

RET_STATUS Read_module_version(struct MVerstruct* MVer)
{
  RET_STATUS STATUS = RET_SUCCESS;

  //1. read UART buffer.
  cleanUARTBuf();

  //2. send CMD
  triple_cmd(R_MODULE_VERSION);

  //3. Receive configure
  STATUS = Module_info((uint8_t *)MVer, sizeof(MVerstruct));
  if(STATUS == RET_SUCCESS)
  {
    Serial.print("  HEAD:     0x");  Serial.println(MVer->HEAD, HEX);
    Serial.print("  Model:    0x");  Serial.println(MVer->Model, HEX);
    Serial.print("  Version:  0x");  Serial.println(MVer->Version, HEX);
    Serial.print("  features: 0x");  Serial.println(MVer->features, HEX);
  }

  return RET_SUCCESS;
}

void Reset_module()
{
  triple_cmd(W_RESET_MODULE);

  WaitAUX_H();
  delay(1000);
}

RET_STATUS SleepModeCmd(uint8_t CMD, void* pBuff)
{
  RET_STATUS STATUS = RET_SUCCESS;

  Serial.print("SleepModeCmd: 0x");  Serial.println(CMD, HEX);
  WaitAUX_H();

  SwitchMode(MODE_3_SLEEP);

  switch (CMD)
  {
    case W_CFG_PWR_DWN_SAVE:
      STATUS = Write_CFG_PDS((struct CFGstruct* )pBuff);
      break;
    case R_CFG:
      STATUS = Read_CFG((struct CFGstruct* )pBuff);
      break;
    case W_CFG_PWR_DWN_LOSE:

      break;
    case R_MODULE_VERSION:
      Read_module_version((struct MVerstruct* )pBuff);
      break;
    case W_RESET_MODULE:
      Reset_module();
      break;

    default:
      return RET_INVALID_PARAM;
  }

  WaitAUX_H();
  return STATUS;
}
//=== Sleep mode cmd ================================-

RET_STATUS SettingModule(struct CFGstruct *pCFG)
{
  RET_STATUS STATUS = RET_SUCCESS;

#ifdef Device_A
  pCFG->ADDH = DEVICE_A_ADDR_H;
  pCFG->ADDL = DEVICE_A_ADDR_L;
#else
  pCFG->ADDH = DEVICE_B_ADDR_H;
  pCFG->ADDL = DEVICE_B_ADDR_L;
#endif

  pCFG->OPTION_bits.trsm_mode =TRSM_FP_MODE;
  pCFG->OPTION_bits.tsmt_pwr = TSMT_PWR_10DB;

  STATUS = SleepModeCmd(W_CFG_PWR_DWN_SAVE, (void* )pCFG);

  SleepModeCmd(W_RESET_MODULE, NULL);

  STATUS = SleepModeCmd(R_CFG, (void* )pCFG);

  return STATUS;
}

RET_STATUS SendMsg(uint8_t timeH, uint8_t timeM, uint8_t timeS, uint32_t numSat, double spedSat, double latitud, double longitud, double altitud, float power)
{
  RET_STATUS STATUS = RET_SUCCESS;

  SwitchMode(MODE_0_NORMAL);

  if(ReadAUX()!=HIGH)
  {
    return RET_NOT_IMPLEMENT;
  }
  delay(10);

  typedef struct info_t {
    uint8_t hh;
    uint8_t mm;
    uint8_t ss;
    uint32_t noSat;
    double speedSat;
    double latSat;
    double lngSat;
    double alti;
    float volt;
  };

#define PACKET_SIZE sizeof(info_t)

  typedef union Data_t {
    info_t info;
    uint8_t packet[PACKET_SIZE];
  };
  Data_t data;
  
  data.info.hh = timeH;
  data.info.mm = timeM;
  data.info.ss = timeS;
  data.info.noSat = numSat;
  data.info.speedSat = spedSat;
  data.info.latSat = latitud;
  data.info.lngSat = longitud;
  data.info.alti = altitud;
  data.info.volt = power;
  
  //TRSM_FP_MODE
  //Send format : ADDH ADDL CHAN DATA_0 DATA_1 DATA_2 ...
#ifdef Device_A
  uint8_t SendBuf[4] = { DEVICE_B_ADDR_H, DEVICE_B_ADDR_L, 0x17, random(0x00, 0x80)};  //for A
#else
  uint8_t SendBuf[PACKET_SIZE + 3];
  SendBuf[0] = DEVICE_A_ADDR_H;
  SendBuf[1] = DEVICE_A_ADDR_L;
  SendBuf[2] = 0x17;
  for (int i = 0; i < PACKET_SIZE; i++)
  {
    SendBuf[i + 3] = data.packet[i];
  }
#endif
  softSerial.write(SendBuf, PACKET_SIZE + 3);

  return STATUS;
}

//The setup function is called once at startup of the sketch
void setup()
{
  RET_STATUS STATUS = RET_SUCCESS;
  struct CFGstruct CFG;
  struct MVerstruct MVer;

  pinMode(M0_PIN, OUTPUT);
  pinMode(M1_PIN, OUTPUT);
  pinMode(AUX_PIN, INPUT);

  softSerial.begin(9600);
  Serial.begin(9600);

  Serial.println("[10-B] ");

  STATUS = SleepModeCmd(R_CFG, (void* )&CFG);
  STATUS = SettingModule(&CFG);

  STATUS = SleepModeCmd(R_MODULE_VERSION, (void* )&MVer);

  // Mode 0 | normal operation
  SwitchMode(MODE_0_NORMAL);

  //self-check initialization.
  WaitAUX_H();
  delay(10);
  
  if(STATUS == RET_SUCCESS)
    Serial.println("Setup init OK!!");
}

// The loop function is called in an endless loop
void loop()
{
  uint8_t h, m, s;
  uint32_t sat;
  double sped, lati, longi, alti;
  float R1 = 120000.0;
  float R2 = 150000.0;
  int reading = analogRead(VOLTAGE_PIN);
  float voltage = reading * (5.0/1024)*((R1 + R2)/R2);

  
  while (Serial.available() > 0){
    gps.encode(Serial.read());
    if (gps.location.isUpdated()){
      h = gps.time.hour() + 7;
      m = gps.time.minute();
      s = gps.time.second();
      sat = gps.satellites.value();
      sped = gps.speed.mps();
      lati = gps.location.lat();
      longi = gps.location.lng();
      alti = gps.altitude.meters();
      
      if(SendMsg(h,m,s,sat,sped,lati,longi,alti, voltage) == RET_SUCCESS)
      {
        
      }
    }
  }
}
