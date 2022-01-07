//--------------------------------------------------------------------------------
//
// SMFDEasy_AYA063 DCC2SIOファンクションデコーダスケッチ
// Copyright(C)'2021 Ayanosuke(Maison de DCC)
// [AYA063_DCC2SIO.ino]
// AYA063用
//
// O0 0      // Atiny85 PB0(5pin)O7 analogwrite head light F0
// O1 1      // Atiny85 PB1(6pin)O6 analogwrite tail light F1
// O2 3      // Atint85 PB3(2pin)O3             sign light F3
// O3 4      // Atiny85 PB4(3pin)O2 analogwrite room light F5
//
// http://maison-dcc.sblo.jp/ http://dcc.client.jp/ http://ayabu.blog.shinobi.jp/
// https://twitter.com/masashi_214
//
// DCC電子工作連合のメンバーです
// https://desktopstation.net/tmi/ https://desktopstation.net/bb/index.php
//
// This software is released under the MIT License.
// http://opensource.org/licenses/mit-license.php
//
//
// 2021/10/23 
// 2021/12/18 Function追加
//--------------------------------------------------------------------------------

// シリアルデバックの有効:1 / 無効:0
#include <arduino.h>
#include "DccCV.h"
#include "NmraDcc.h"
#include <avr/eeprom.h>	 //required by notifyCVRead() function if enabled below
#include <SoftwareSerial.h>

//各種設定、宣言
#define DECODER_ADDRESS 3
#define DCC_ACK_PIN 0   // Atiny85 PB0(5pin) if defined enables the ACK pin functionality. Comment out to disable.
//                      // Atiny85 DCCin(7pin)
#define O1 0            // Atiny85 PB0(5pin)
#define O2 1            // Atiny85 PB1(6pin) analogwrite
#define O3 3            // Atint85 PB3(2pin)
#define O4 4            // Atiny85 PB4(3pin) analogwrite

#define F0 0
#define F1 1
#define F2 2
#define F3 3
#define F4 4
#define F5 5
#define F6 6
#define F7 7
#define F8 8
#define F9 9
#define F10 10
#define F11 11
#define F12 12

#define ON 1
#define OFF 0

// ファンクション CV変換テーブル
#define CV_F0 33
#define CV_F1 35
#define CV_F2 36
#define CV_F3 37
#define CV_F4 38
#define CV_F5 39
#define CV_F6 40
#define CV_F7 41
#define CV_F8 42
#define CV_F9 43
#define CV_F10 44
#define CV_F11 45
#define CV_F12 46

//使用クラスの宣言
NmraDcc	 Dcc;
DCC_MSG	 Packet;

SoftwareSerial mySerial(O1, O2); // RX, TX

//Internal variables and other.
#if defined(DCC_ACK_PIN)
const int DccAckPin = DCC_ACK_PIN ;
#endif

struct CVPair {
  uint16_t	CV;
  uint8_t	Value;
};
CVPair FactoryDefaultCVs [] = {
  {CV_MULTIFUNCTION_PRIMARY_ADDRESS, DECODER_ADDRESS}, // CV01
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, 0},               // CV09 The LSB is set CV 1 in the libraries .h file, which is the regular address location, so by setting the MSB to 0 we tell the library to use the same address as the primary address. 0 DECODER_ADDRESS
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB, 0},          // CV17 XX in the XXYY address
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB, 0},          // CV18 YY in the XXYY address
  {CV_29_CONFIG, 2},                                   // CV29 Make sure this is 0 or else it will be random based on what is in the eeprom which could caue headaches
  {CV_F0 ,0},
  {CV_F1 ,0},
  {CV_F2 ,0},
  {CV_F3 ,3}, // Room Light LED [O3]
  {CV_F4 ,4}, // Panta Spark LED [O4]
  {CV_F5 ,0},
  {CV_F6 ,0},
  {CV_F7 ,0},
  {CV_F8 ,0},
  {CV_F9 ,0},
  {CV_F10 ,0},
  {CV_F11 ,0},
  {CV_F12 ,0},   
};

void(* resetFunc) (void) = 0;  //declare reset function at address 0
uint8_t FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);

void notifyCVResetFactoryDefault()
{
  //When anything is writen to CV8 reset to defaults.
  resetCVToDefault();
  //Serial.println("Resetting...");
  delay(1000);  //typical CV programming sends the same command multiple times - specially since we dont ACK. so ignore them by delaying

  resetFunc();
};

//------------------------------------------------------------------
// CVをデフォルトにリセット
// Serial.println("CVs being reset to factory defaults");
//------------------------------------------------------------------
void resetCVToDefault()
{
  for (int j = 0; j < FactoryDefaultCVIndex; j++ ) {
    Dcc.setCV( FactoryDefaultCVs[j].CV, FactoryDefaultCVs[j].Value);
  }
};

//------------------------------------------------------------------
// CVが変更された
//------------------------------------------------------------------
extern void	   notifyCVChange( uint16_t CV, uint8_t Value) {
};

//------------------------------------------------------------------
// CV Ack
// Smile Function Decoder は未対応
//------------------------------------------------------------------
void notifyCVAck(void)
{
  //Serial.println("notifyCVAck");
  digitalWrite(O3,HIGH);
  digitalWrite(O4,HIGH);
  delay( 6 );
  digitalWrite(O3,LOW);
  digitalWrite(O4,LOW);
}

//------------------------------------------------------------------
// Arduino固有の関数 setup() :初期設定
//------------------------------------------------------------------
void setup()
{
  uint8_t cv_value;

  pinMode(O1, OUTPUT);    // ソフトシリアル用 
  pinMode(O2, OUTPUT);  //
  pinMode(O3, OUTPUT);
  pinMode(O4, OUTPUT);
  digitalWrite(O1, OFF);
  digitalWrite(O2, OFF);
  digitalWrite(O3, OFF);
  digitalWrite(O4, OFF);

  mySerial.begin(9600);
  mySerial.println(" ");
//  mySerial.println("Hello,SFM");

  Dccinit();

  //Reset task
  gPreviousL5 = millis();
}


//---------------------------------------------------------------------
// Arduino Main Loop
//---------------------------------------------------------------------
void loop() {
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

  if ( (millis() - gPreviousL5) >= 10) // 100:100msec  10:10msec  Function decoder は 10msecにしてみる。
  {
    SIO_Control();

    //Reset task
    gPreviousL5 = millis();
  }
}


//---------------------------------------------------------------------
// SIO_Control()
//---------------------------------------------------------------------
void SIO_Control()
{
  static char Fnc[15]={0,0,0,0,0,0,0,0,0,0,0};
  static char state = 0;

  if(state == 0 ){
    Fnc[1] = gState_F1; 
    Fnc[2] = gState_F2; 
    Fnc[3] = gState_F3; 
    Fnc[4] = gState_F4; 
    Fnc[5] = gState_F5; 
    Fnc[6] = gState_F6; 
    Fnc[7] = gState_F7;
    Fnc[8] = gState_F8; 
    Fnc[9] = gState_F9;
    Fnc[10] = gState_F10; 

    state = 1;
  }

  if(gState_F1 != Fnc[1]){
    Fnc[1]=gState_F1;
    mySerial.println("1"); // hone

  } else  if(gState_F2 != Fnc[2]){
    Fnc[2]=gState_F2;
    mySerial.println("2"); // 停止

  } else  if(gState_F3 != Fnc[3]){
    Fnc[3]=gState_F3;
    mySerial.println("3"); // 高速走行

  } else if(gState_F4 != Fnc[4]){
    Fnc[4]=gState_F4;
    mySerial.println("4"); // オープニング

  } else if(gState_F5 != Fnc[5]){
    Fnc[5]=gState_F5;
    mySerial.println("5"); // プロローグ

  } else if(gState_F6 != Fnc[6]){
    Fnc[6]=gState_F6;
    mySerial.println("6"); // 夜に駆ける/YOASOBI

  } else if(gState_F7 != Fnc[7]){
    Fnc[7]=gState_F7;
    mySerial.println("7"); // アイノカタチfeat.HIDE(GReeeeN)/MISIA

  } else if(gState_F8 != Fnc[8]){
    Fnc[8]=gState_F8;
    mySerial.println("8"); // 天体観測/BUMP OF CHICKEN

  } else if(gState_F9 != Fnc[9]){
    Fnc[9]=gState_F9;
    mySerial.println("9"); // 音量-

  } else if(gState_F10 != Fnc[10]){
    Fnc[10]=gState_F10;
    mySerial.println("A"); // 音量+
  }
}



//---------------------------------------------------------------------------
// DCC速度信号の受信によるイベント 
//---------------------------------------------------------------------------
extern void notifyDccSpeed( uint16_t Addr, uint8_t Speed, uint8_t ForwardDir, uint8_t MaxSpeed )
{
}

//---------------------------------------------------------------------------
// ファンクション信号受信のイベント
// FN_0_4とFN_5_8は常時イベント発生（DCS50KはF8まで）
// FN_9_12以降はFUNCTIONボタンが押されたときにイベント発生
// 前値と比較して変化あったら処理するような作り。
// 分かりづらい・・・
//---------------------------------------------------------------------------
extern void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState)
{
  if( FuncGrp == FN_0_4) // F0〜F4の解析
  {
    if( gState_F0 != (FuncState & FN_BIT_00))
    {
      //Get Function 0 (FL) state
      gState_F0 = (FuncState & FN_BIT_00);
    }
    if( gState_F1 != (FuncState & FN_BIT_01))
    {
      //Get Function 1 state
      gState_F1 = (FuncState & FN_BIT_01);
    }
    if( gState_F2 != (FuncState & FN_BIT_02))
    {
      gState_F2 = (FuncState & FN_BIT_02);
    }
    if( gState_F3 != (FuncState & FN_BIT_03))
    {
      gState_F3 = (FuncState & FN_BIT_03);
    }
    if( gState_F4 != (FuncState & FN_BIT_04))
    {
      gState_F4 = (FuncState & FN_BIT_04);
    }
  }

  if( FuncGrp == FN_5_8)  // F5〜F8の解析
  {
    if( gState_F5 != (FuncState & FN_BIT_05))
    {
      //Get Function 0 (FL) state
      gState_F5 = (FuncState & FN_BIT_05);
    }
    if( gState_F6 != (FuncState & FN_BIT_06))
    {
      //Get Function 1 state
      gState_F6 = (FuncState & FN_BIT_06);
    }
    if( gState_F7 != (FuncState & FN_BIT_07))
    {
      gState_F7 = (FuncState & FN_BIT_07);
    }
    if( gState_F8 != (FuncState & FN_BIT_08))
    {
      gState_F8 = (FuncState & FN_BIT_08);
    }
  }


  if( FuncGrp == FN_9_12)
  {
    if( gState_F9 != (FuncState & FN_BIT_09))
    {
      gState_F9 = (FuncState & FN_BIT_09);
    }
    if( gState_F10 != (FuncState & FN_BIT_10))
    {
      gState_F10 = (FuncState & FN_BIT_10);
    }
    if( gState_F11 != (FuncState & FN_BIT_11))
    {
      gState_F11 = (FuncState & FN_BIT_11);
    }
    if( gState_F12 != (FuncState & FN_BIT_12))
    {
      gState_F12 = (FuncState & FN_BIT_12);
    }
  }
}


//------------------------------------------------------------------
// DCC初期化処理）
//------------------------------------------------------------------
void Dccinit(void)
{

  //DCCの応答用負荷ピン
#if defined(DCCACKPIN)
  //Setup ACK Pin
  pinMode(DccAckPin, OUTPUT);
  digitalWrite(DccAckPin, 0);
#endif

#if !defined(DECODER_DONT_DEFAULT_CV_ON_POWERUP)
  if ( Dcc.getCV(CV_MULTIFUNCTION_PRIMARY_ADDRESS) == 0xFF ) {   //if eeprom has 0xFF then assume it needs to be programmed
    notifyCVResetFactoryDefault();
  } else {
    //Serial.println("CV Not Defaulting");
  }
#else
  //Serial.println("CV Defaulting Always On Powerup");
  notifyCVResetFactoryDefault();
#endif

  // Setup which External Interrupt, the Pin it's associated with that we're using, disable pullup.
  Dcc.pin(0, 2, 0); // Atiny85 7pin(PB2)をDCC_PULSE端子に設定

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, 100,   FLAGS_MY_ADDRESS_ONLY , 0 );

  //Init CVs
  gCV1_SAddr = Dcc.getCV( CV_MULTIFUNCTION_PRIMARY_ADDRESS ) ;
  gCVx_LAddr = (Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB ) << 8) + Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB );
}



#if 0
//-----------------------------------------------------
// F4 を使って、メッセージを表示させる。
//上位ビットから吐き出す
// ex 5 -> 0101 -> ー・ー・
//-----------------------------------------------------
void LightMes( char sig ,char set)
{
  char cnt;
  for( cnt = 0 ; cnt<set ; cnt++ ){
    if( sig & 0x80){
      digitalWrite(O1, HIGH); // 短光
      delay(200);
      digitalWrite(O1, LOW);
      delay(200);
    } else {
      digitalWrite(O1, HIGH); // 長光
      delay(1000);
      digitalWrite(O1, LOW);            
      delay(200);
    }
    sig = sig << 1;
  }
      delay(400);
}

//-----------------------------------------------------
// Debug用Lﾁｶ
//-----------------------------------------------------
void pulse()
{
  digitalWrite(O1, HIGH); // 短光
  delay(100);
  digitalWrite(O1, LOW);
}
#endif
