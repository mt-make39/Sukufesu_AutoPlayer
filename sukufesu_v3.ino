#include <Keyboard.h>
#include <MsTimer2.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#define ON 1
#define OFF 0
#define SW1 14
#define SW2 15
#define SW3 16
#define SW4 17
#define CS 10

#define BUF_SIZE 5
#define FILE_MAX 99
#define SELECT 1
#define PLAYING 2
#define PAUSE 3
#define TEST 4

File myFile;

volatile bool openFlg = 0, dispFlg = 0;
unsigned int fileNum = 1;
unsigned int pressOnTime[9] = {0}; //各ノーツの残りタップ時間
unsigned int tapTime = 0;   //タップする時間[ms]
unsigned long elapsedTime = 0; //スタートからの経過時間[ms]
unsigned long nextTime = 0; //次のノーツまでの時間[ms]
unsigned int twinTap = 0; //同時押しか否か 0 or 1
unsigned int notesNum = 0; //ノーツの番号 1～9
unsigned char state = SELECT, fileNumBuf[BUF_SIZE];
String fileName;

void setup()
{
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  pinMode(SW3, INPUT);
  pinMode(SW4, INPUT);
  //Serial.begin(9600);
  Wire.begin();       // I2C初期化
  MsTimer2::set(1, interruptTimer);    //タイマ割り込み設定1ms周期
  MsTimer2::start();                   //タイマ割り込み許可
  LCD_begin();
  writeCmd(0x08 | 0x04);
  lcdPrint("Initializing    ");
  setCursor(0, 1);
  lcdPrint("      SD card...");
  if (!SD.begin(CS)) {
    lcdClear();
    lcdPrint("Initialization  ");
    setCursor(0, 1);
    lcdPrint("         failed!");
    while (1);
  }
  delay(1000);
  lcdClear();
}

void loop()
{
  switch (state) {
    case SELECT:
      //servo_init();
      servoMove();
      lcdSelectMode();
      /*if (openFlg) {
        openFlg = 0;
        fileName = "file";
        fileName += itoa(fileNum, fileNumBuf, 10);
        fileName += ".csv";
        myFile = SD.open(fileName); //選択したファイルを開く
        if (myFile)readElapsedTime();
        }*/
      break;
    case PLAYING:
      if (openFlg) {
        openFlg = 0;
        fileName = "file";
        fileName += itoa(fileNum, fileNumBuf, 10);
        fileName += ".csv";
        myFile = SD.open(fileName); //選択したファイルを開く
        if (myFile)readElapsedTime();
      }
      if (myFile) {
        while ((myFile.available()) && (state == PLAYING)) { //データの終わりまでループ
          if (elapsedTime >= nextTime) {
            twinTap = readTwinTap();
            tapTime = readTapTime();
            notesNum = readNotesNum();
            if (tapTime == 2000)tapTime = 75;
            pressOnTime[notesNum - 1] = tapTime; //タップするノーツに時間をセット
            if (twinTap == 1) {
              //同時押しならもう一行読み込む
              //経過時間を空読み
              readElapsedTime();
              twinTap = readTwinTap();
              tapTime = readTapTime();
              notesNum = readNotesNum();
              if (tapTime == 2000)tapTime = 75;
              pressOnTime[notesNum - 1] = tapTime; //タップするノーツに時間をセット
            }
            nextTime = readElapsedTime();
          }
          servoMove();
          /*if (dispFlg) {
            dispFlg = 0;
            lcdPlayingMode();
            }*/
        }
        if (state == PLAYING) {
          myFile.close();
          state = SELECT;
        }
      } else {
        setCursor(0, 0);
        lcdPrint(" File Not Found ");
        setCursor(0, 1);
        lcdPrint("                ");
        delay(1000);
        state = SELECT;
      }
      break;
    case PAUSE:
      lcdPauseMode();
      break;
    case TEST:
      break;
  }
}

void interruptTimer()
{
  static unsigned char swCheckCnt = 0, readCheckCnt = 0;
  static bool nowSW1 = OFF, oldSW1 = OFF;
  static bool nowSW2 = OFF, oldSW2 = OFF;
  static bool nowSW3 = OFF, oldSW3 = OFF;
  static bool nowSW4 = OFF, oldSW4 = OFF;

  if (state == PLAYING) {
    elapsedTime++;
  }
  for (int i = 0; i < 9; i++) {
    if (pressOnTime[i] > 0)pressOnTime[i]--;
  }

  swCheckCnt++;
  if (swCheckCnt > 20) {
    swCheckCnt = 0;
    nowSW1 = !digitalRead(SW1);
    nowSW2 = !digitalRead(SW2);
    nowSW3 = !digitalRead(SW3);
    nowSW4 = !digitalRead(SW4);
    if ((nowSW1 == ON) && (oldSW1 == OFF)) {
      switch (state) {
        case SELECT:
          state = PLAYING;
          openFlg = 1;
          dispFlg = 1;
          elapsedTime = 0;
          nextTime = 0;
          break;
        case PLAYING:
          break;
        case PAUSE:
          state = PLAYING;
          dispFlg = 1;
          break;
        default:
          break;
      }
    }
    if ((nowSW2 == ON) && (oldSW2 == OFF)) {
      switch (state) {
        case SELECT:
          if (!myFile)openFlg = 1;
          break;
        case PLAYING:
          state = PAUSE;
          break;
        case PAUSE:
          state = SELECT;
          myFile.close();
          break;
        default:
          break;
      }
    }
    if ((nowSW3 == ON) && (oldSW3 == OFF)) {
      switch (state) {
        case SELECT:
          if (myFile)myFile.close();
          fileNum++;
          if (fileNum > FILE_MAX)fileNum = 1;
          break;
        case PLAYING:
          elapsedTime += 10;
          break;
        default:
          break;
      }
    }
    if ((nowSW4 == ON) && (oldSW4 == OFF)) {
      switch (state) {
        case SELECT:
          if (myFile)myFile.close();
          fileNum--;
          if (fileNum < 1)fileNum = FILE_MAX;
          break;
        case PLAYING:
          if (elapsedTime > 10)elapsedTime -= 10;
          break;
        default:
          break;
      }
    }
    oldSW1 = nowSW1;
    oldSW2 = nowSW2;
    oldSW3 = nowSW3;
    oldSW4 = nowSW4;
  }
}

void servo_init(void)
{
  notes1.write(N1_NTAP);
  notes2.write(N2_NTAP);
  notes3.write(N3_NTAP);
  delay(50);
  notes4.write(N4_NTAP);
  notes5.write(N5_NTAP);
  notes6.write(N6_NTAP);
  delay(50);
  notes7.write(N7_NTAP);
  notes8.write(N8_NTAP);
  notes9.write(N9_NTAP);
  delay(50);
}

void servo_test(void)
{
  notes1.write(N1_TAP);
  notes2.write(N2_TAP);
  notes3.write(N3_TAP);
  delay(50);
  notes4.write(N4_TAP);
  notes5.write(N5_TAP);
  notes6.write(N6_TAP);
  delay(50);
  notes7.write(N7_TAP);
  notes8.write(N8_TAP);
  notes9.write(N9_TAP);
  delay(50);
}

void servoMove(void)
{

  if (pressOnTime[8] > 0)notes1.write(N1_TAP);
  else notes1.write(N1_NTAP);

  //notes2
  if (pressOnTime[7] > 0)notes2.write(N2_TAP);
  else notes2.write(N2_NTAP);

  //notes3
  if (pressOnTime[6] > 0)notes3.write(N3_TAP);
  else notes3.write(N3_NTAP);

  //notes4
  if (pressOnTime[5] > 0)notes4.write(N4_TAP);
  else notes4.write(N4_NTAP);

  //notes5
  if (pressOnTime[4] > 0)notes5.write(N5_TAP);
  else notes5.write(N5_NTAP);

  //notes6
  if (pressOnTime[3] > 0)notes6.write(N6_TAP);
  else notes6.write(N6_NTAP);

  //notes7
  if (pressOnTime[2] > 0)notes7.write(N7_TAP);
  else notes7.write(N7_NTAP);

  //notes8
  if (pressOnTime[1] > 0)notes8.write(N8_TAP);
  else notes8.write(N8_NTAP);

  //notes9
  if (pressOnTime[0] > 0)notes9.write(N9_TAP);
  else notes9.write(N9_NTAP);
}

unsigned long readElapsedTime(void)
{
  char temp;
  unsigned char i = 0;
  unsigned long buf[7] = {0};
  while (true) { //数字以外まで1バイトずつ読み込み
    temp = myFile.read();
    if ((temp >= 0x30) && (temp <= 0x39)) { //数字かチェック
      buf[i] = (unsigned long)(temp - '0');
      i++;
      if (i == 7)break;
    } else {
      break;
    }
  }
  if (i > 0) {
    unsigned long readNum = 0;
    switch (i) {
      case 1:
        readNum = buf[0];
        break;
      case 2:
        readNum = buf[0] * 10 + buf[1];
        break;
      case 3:
        readNum = buf[0] * 100 + buf[1] * 10 + buf[2];
        break;
      case 4:
        readNum = buf[0] * 1000 + buf[1] * 100 + buf[2] * 10 + buf[3];
        break;
      case 5:
        readNum = buf[0] * 10000 + buf[1] * 1000 + buf[2] * 100 + buf[3] * 10 + buf[4];
        break;
      case 6:
        readNum = buf[0] * 100000 + buf[1] * 10000 + buf[2] * 1000 + buf[3] * 100 + buf[4] * 10 + buf[5];
        break;
      case 7:
        readNum = buf[0] * 1000000 + buf[1] * 100000 + buf[2] * 10000 + buf[3] * 1000 + buf[4] * 100 + buf[5] * 10 + buf[6];
        break;
      default:
        break;
    }
    return readNum;
  } else {
    return 0;
  }
}

unsigned char readTwinTap(void)
{
  char temp;
  unsigned int i = 0, buf[2] = {0};
  while (true) { //数字以外まで1バイトずつ読み込み
    temp = myFile.read();
    if ((temp >= 0x30) && (temp <= 0x39)) { //数字かチェック
      buf[i] = (unsigned int)(temp - '0');
      i++;
      if (i == 2)break;
    } else {
      break;
    }
  }
  if (i > 0) {
    unsigned int readNum = 0;
    switch (i) {
      case 1:
        readNum = buf[0];
        break;
      case 2:
        readNum = buf[0] * 10 + buf[1];
        break;
      default:
        break;
    }
    return readNum;
  } else {
    return 0;
  }
}

unsigned long readTapTime(void)
{
  char temp;
  unsigned int i = 0, buf[5] = {0};
  while (true) { //数字以外まで1バイトずつ読み込み
    temp = myFile.read();
    if ((temp >= 0x30) && (temp <= 0x39)) { //数字かチェック
      buf[i] = (unsigned int)(temp - '0');
      i++;
      if (i == 5)break;
    } else {
      break;
    }

  }
  if (i > 0) {
    unsigned int readNum = 0;
    switch (i) {
      case 1:
        readNum = buf[0];
        break;
      case 2:
        readNum = buf[0] * 10 + buf[1];
        break;
      case 3:
        readNum = buf[0] * 100 + buf[1] * 10 + buf[2];
        break;
      case 4:
        readNum = buf[0] * 1000 + buf[1] * 100 + buf[2] * 10 + buf[3];
        break;
      case 5:
        readNum = buf[0] * 10000 + buf[1] * 1000 + buf[2] * 100 + buf[3] * 10 + buf[4];
        break;
      default:
        break;
    }
    return readNum;
  } else {
    return 0;
  }
}

unsigned char readNotesNum(void)
{
  char temp;
  unsigned int i = 0, buf[2] = {0};
  while (true) { //数字以外まで1バイトずつ読み込み
    temp = myFile.read();
    if ((temp >= 0x30) && (temp <= 0x39)) { //数字かチェック
      buf[i] = (unsigned int)(temp - '0');   //char型だと上手く計算出来ない
      i++;
      if (i == 2)break;
    } else if (temp == 0x0d) {    //改行コードが2バイトの為、1バイト空読み
      temp = myFile.read();
      break;
    } else {
      break;
    }
  }
  if (i > 0) {
    unsigned int readNum = 0;
    switch (i) {
      case 1:
        readNum = buf[0];
        break;
      case 2:
        readNum = buf[0] * 10 + buf[1];
        break;
      default:
        break;
    }
    return readNum;
  } else {
    return 0;
  }
}

void lcdSelectMode(void)
{
  setCursor(0, 0);
  lcdPrint("File Number:");
  lcdPrint(itoa(fileNum, fileNumBuf, 10));
  if (fileNum < 10)lcdPrint("   ");
  else lcdPrint("   ");
  setCursor(0, 1);
  if (!myFile)lcdPrint(" [ SelectFile ] ");
  else  lcdPrint(" [ FileSet OK ] ");
}

void lcdPlayingMode(void)
{
  setCursor(0, 0);
  lcdPrint("File Number:");
  lcdPrint(itoa(fileNum, fileNumBuf, 10));
  if (fileNum < 10)lcdPrint(" ");
  setCursor(0, 1);
  lcdPrint(" [ NowPlaying ] ");
}


void lcdPauseMode(void)
{
  setCursor(0, 0);
  lcdPrint("File Number:");
  lcdPrint(itoa(fileNum, fileNumBuf, 10));
  if (fileNum < 10)lcdPrint(" ");
  setCursor(0, 1);
  lcdPrint(" [ Pause ]      ");
}

void lcdClear(void)
{
  writeCmd(0x01);     // クリア・ディスプレイ・コマンド
}

void setCursor(byte col, byte row)
{
  byte row_offsets[] = { 0x00, 0x40 };
  if ( row > 1 ) {
    row = 1;
  }
  writeCmd( 0x80 | (col + row_offsets[row]) );
}

void lcdPrint(char *str)
{
  byte i;
  for (i = 0; i < 16; i++) {
    if (str[i] == 0x00) {
      break;
    }
    else {
      writeData(str[i]);
    }
  }
}

void LCD_begin(void)
{
  // LCD初期化
  delay(15);
  writeCmd(0x01);//クリア　ディスプレイ
  writeCmd(0x38);//８ビットモード、２ライン、５ｘ８ドット
  writeCmd(0x0f);//ディスプレイＯＮ、CURSOR-ON、blinking-ON
  writeCmd(0x06);//CURSOR移動、スクロールOFF
}

void writeCmd(uint8_t cmd)
{
  uint8_t rs_flg;
  Wire.beginTransmission(0x50);
  rs_flg = 0x00;
  Wire.write(rs_flg);
  Wire.write(cmd);
  Wire.endTransmission();
  delay(5);
}

void writeData(uint8_t dat)
{
  Wire.beginTransmission(0x50);
  Wire.write(0x80);
  Wire.write(dat);
  Wire.endTransmission();
  delay(5);
}
