
#include <SPI.h>

//Add the SdFat Libraries
#include <SdFat.h>
#include <FreeStack.h>

//and the MP3 Shield Library
#include <SFEMP3Shield.h>

// Below is not needed if interrupt driven. Safe to remove if not using.
#if defined(USE_MP3_REFILL_MEANS) && USE_MP3_REFILL_MEANS == USE_MP3_Timer1
#include <TimerOne.h>
#elif defined(USE_MP3_REFILL_MEANS) && USE_MP3_REFILL_MEANS == USE_MP3_SimpleTimer
#include <SimpleTimer.h>
#endif


#define IR_PIN      5    // 赤外線受信モジュール接続ピン番号
#define DATA_POINT  3     // 受信したデータから読取る内容のデータ位置


SdFat sd;
SFEMP3Shield MP3player;

int16_t last_ms_char; // milliseconds of last recieved character from Serial port.
int8_t buffer_pos; // next position to recieve character from Serial port.

//------------------------------------------------------------------------------

char buffer[6]; // 0-35K+null

void setup() {

  uint8_t result; //result code from some function as to be tested at later time.

  Serial.begin(9600);

  //  Serial.print(F("F_CPU = "));
  //  Serial.println(F_CPU);
  //  Serial.print(F("Free RAM = ")); // available in Version 1.0 F() bases the string to into Flash, to use less SRAM.
  //  Serial.print(FreeStack(), DEC);  // FreeRam() is provided by SdFatUtil.h
  //  Serial.println(F(" Should be a base line of 1017, on ATmega328 when using INTx"));

  pinMode(IR_PIN, INPUT) ; // 赤外線受信モジュールに接続ピンをデジタル入力に設定

  //  //Initialize the SdCard.
  if (!sd.begin(SD_SEL, SPI_FULL_SPEED)) sd.initErrorHalt();
  // depending upon your SdCard environment, SPI_HAVE_SPEED may work better.
  if (!sd.chdir("/")) sd.errorHalt("sd.chdir");
  //
  //  //Initialize the MP3 Player Shield
  result = MP3player.begin();
  //
  //  Serial.print("result :");
  //  Serial.println(result);
  //
  //    //check result, see readme for error codes.
  //    if (result != 0) {
  //      Serial.print(F("Error code: "));
  //      Serial.print(result);
  //      Serial.println(F(" when trying to start MP3 player"));
  //      if (result == 6) {
  //        Serial.println(F("Warning: patch file not found, skipping.")); // can be removed for space, if needed.
  //        Serial.println(F("Use the \"d\" command to verify SdCard can be read")); // can be removed for space, if needed.
  //      }
  //    }

#if (0)
  // Typically not used by most shields, hence commented out.
  Serial.println(F("Applying ADMixer patch."));
  if (MP3player.ADMixerLoad("admxster.053") == 0) {
    Serial.println(F("Setting ADMixer Volume."));
    MP3player.ADMixerVol(-3);
  }
#endif

  //  help();
  last_ms_char = millis(); // stroke the inter character timeout.
  buffer_pos = 0; // start the command string at zero length.


  // 音量設定（default）
  union twobyte mp3_vol; // create key_command existing variable that can be both word and double byte of left and right.
  mp3_vol.word = MP3player.getVolume(); // returns a double uint8_t of Left and Right packed into int16_t
  // assume equal balance and use byte[1] for math
  mp3_vol.byte[1] = 0x00;
  // push byte[1] into both left and right assuming equal balance.
  MP3player.setVolume(mp3_vol.byte[1], mp3_vol.byte[1]); // commit new volume
  Serial.print(F("Volume changed to -"));
  Serial.print(mp3_vol.byte[1] >> 1, 1);
  Serial.println(F("[dB]"));

}

//------------------------------------------------------------------------------

void SerialPrintPaddedNumber(int16_t value, int8_t digits ) {
  int currentMax = 10;
  for (byte i = 1; i < digits; i++) {
    if (value < currentMax) {
      Serial.print("0");
    }
    currentMax *= 10;
  }
  Serial.print(value);
}


//------------------------------------------------------------------------------

int IRrecive()
{
  unsigned long t ;
  int i , j ;
  int cnt , ans ;
  char IRbit[64] ;
  static bool flag = false;
  ans = 0 ;
  t = 0 ;

  // ボタンを押した時の条件分岐
  if (digitalRead(IR_PIN) == LOW) {
    // リーダ部のチェックを行う
    t = micros() ;                          // 現在の時刻(us)を得る
    while (digitalRead(IR_PIN) == LOW) ;  // HIGH(ON)になるまで待つ
    t = micros() - t ;          // LOW(OFF)の部分をはかる
    //    Serial.println("準備完了");
  }
  // リーダ部有りなら処理する(3.4ms以上のLOWにて判断する)
  if (t >= 3400) {
    if (flag == false) {
      //      Serial.println("到達");
      flag = true;
    }
    i = 0 ;
    while (digitalRead(IR_PIN) == HIGH) ; // ここまでがリーダ部(ON部分)読み飛ばす
    // データ部の読み込み
    while (1) {
      while (digitalRead(IR_PIN) == LOW) ; // OFF部分は読み飛ばす
      t = micros() ;
      cnt = 0 ;
      while (digitalRead(IR_PIN) == HIGH) { // LOW(OFF)になるまで待つ
        delayMicroseconds(10) ;
        cnt++ ;
        if (cnt >= 1200) break ;    // 12ms以上HIGHのままなら中断
      }
      t = micros() - t ;
      if (t >= 10000) break ;      // ストップデータ
      if (t >= 1000)  IRbit[i] = (char)0x31 ;  // ON部分が長い
      else            IRbit[i] = (char)0x30 ;  // ON部分が短い
      i++ ;
    }
    // データ有りなら指定位置のデータを取り出す
    if (i != 0) {
      i = (DATA_POINT - 1) * 8 ;
      for (j = 0 ; j < 8 ; j++) {
        if (IRbit[i + j] == 0x31) bitSet(ans, j) ;
      }
    }
  }

  // 周波数を返す
  return (ans) ;
}

//------------------------------------------------------------------------------

void loop() {
  int ans ; // リモコンで押した番号 ex) 1番→45(0x45)
  char inByte;

  // Below is only needed if not interrupt driven. Safe to remove if not using.
  //#if defined(USE_MP3_REFILL_MEANS) \
  //    && ( (USE_MP3_REFILL_MEANS == USE_MP3_SimpleTimer) \
  //    ||   (USE_MP3_REFILL_MEANS == USE_MP3_Polled)      )
  //    MP3player.available();
  //#endif

  ans = IRrecive();

  // ボタンが押された時
  if (ans != 0) {
    Serial.println("ボタン受け付けました");
    Serial.println(ans, HEX) ;
    switch (ans) {
      case 0x45:
        inByte = '1';
        Serial.println("track001.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(1);
        break;
      case 0x46:
        inByte = '2';
        Serial.println("track002.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(2);
        break;
      case 0x47:
        inByte = '3';
        Serial.println("track003.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(3);
        break;
      case 0x44:
        inByte = '4';
        Serial.println("track004.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(4);
        break;
      case 0x40:
        inByte = '5';
        Serial.println("track005.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(5);
        break;
      case 0x43:
        inByte = '6';
        Serial.println("track006.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(6);
        break;
      case 0x07:
        inByte = '7';
        Serial.println("track007.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(7);
        break;
      case 0x15:
        inByte = '8';
        Serial.println("track008.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(8);
        break;
      case 0x09:
        inByte = '9';
        Serial.println("track009.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(9);
        break;
      case 0x19:
        inByte = '0';
        Serial.println("track010.mp3");
        MP3player.stopTrack();
        delay(500);
        MP3player.playTrack(10);
        break;
      case 0x1C:
        inByte = 'C';
        Serial.println("停止");
        MP3player.stopTrack();
        break;
    }
  }
  else {
    //      Serial.println("Nodata");
  }



  if ((0x20 <= inByte) && (inByte <= 0x126)) { // strip off non-ASCII, such as CR or LF
    if (isDigit(inByte)) { // macro for ((inByte >= '0') && (inByte <= '9'))
      // else if it is a number, add it to the string
      buffer[buffer_pos++] = inByte;
    }

    buffer[buffer_pos] = 0; // update end of line
    last_ms_char = millis(); // stroke the inter character timeout.
  }


  if ((millis() - last_ms_char) > 500 && ( buffer_pos > 0 )) {
    // ICT expired and have something
    if (buffer_pos == 1) {
      //      MP3player.playTrack(inByte);
    } else if (buffer_pos > 5) {
      // dump if entered command is greater then uint16_t
      Serial.println(F("Ignored, Number is Too Big!"));
    } else {
      // otherwise its a number, scan through files looking for matching index.
      int16_t fn_index = atoi(buffer);
      SdFile file;
      char filename[13];
      sd.chdir("/", true);
      uint16_t count = 1;
      while (file.openNext(sd.vwd(), O_READ)) {
        file.getName(filename, sizeof(filename));
        if ( isFnMusic(filename) ) {
          if (count == fn_index) {
            Serial.print(F("Index "));
            SerialPrintPaddedNumber(count, 5 );
            Serial.print(F(": "));
            Serial.println(filename);
            Serial.print(F("Playing filename: "));
            Serial.println(filename);
            int8_t result = MP3player.playMP3(filename);
            //check result, see readme for error codes.
            if (result != 0) {
              Serial.print(F("Error code: "));
              Serial.print(result);
              Serial.println(F(" when trying to play track"));
            }
            char title[30]; // buffer to contain the extract the Title from the current filehandles
            char artist[30]; // buffer to contain the extract the artist name from the current filehandles
            char album[30]; // buffer to contain the extract the album name from the current filehandles
            MP3player.trackTitle((char*)&title);
            MP3player.trackArtist((char*)&artist);
            MP3player.trackAlbum((char*)&album);

            //print out the arrays of track information
            //            Serial.write((byte*)&title, 30);
            //            Serial.println();
            //            Serial.print(F("by:  "));
            //            Serial.write((byte*)&artist, 30);
            //            Serial.println();
            //            Serial.print(F("Album:  "));
            //            Serial.write((byte*)&album, 30);
            //            Serial.println();
            break;
          }
          count++;
        }
        file.close();
      }

    }

    //reset buffer to start over
    buffer_pos = 0;
    buffer[buffer_pos] = 0; // delimit
  }

}

uint32_t  millis_prv;

//------------------------------------------------------------------------------
/**
   \brief Decode the Menu.

   Parses through the characters of the users input, executing corresponding
   MP3player library functions and features then displaying a brief menu and
   prompting for next input command.
*/
void parse_menu(byte key_command) {

  uint8_t result; // result code from some function as to be tested at later time.

  // Note these buffer may be desired to exist globably.
  // but do take much space if only needed temporarily, hence they are here.
  //  char title[30]; // buffer to contain the extract the Title from the current filehandles
  //  char artist[30]; // buffer to contain the extract the artist name from the current filehandles
  //  char album[30]; // buffer to contain the extract the album name from the current filehandles

  //  Serial.print(F("Received command: "));
  //  Serial.write(key_command);
  //  Serial.println(F(" "));

  //if s, stop the current track
  //  if (key_command == 's') {
  //    Serial.println(F("Stopping"));
  //    MP3player.stopTrack();

  //if 1-9, play corresponding track
  //  }

  //  else if (key_command >= '1' && key_command <= '9') {
  //    //convert ascii numbers to real numbers
  //    key_command = key_command - 48;
  //
  //#if USE_MULTIPLE_CARDS
  //    sd.chvol(); // assign desired sdcard's volume.
  //#endif
  //    //tell the MP3 Shield to play a track
  //    result = MP3player.playTrack(key_command);
  //
  //    //check result, see readme for error codes.
  //    if (result != 0) {
  //      Serial.print(F("Error code: "));
  //      Serial.print(result);
  //      Serial.println(F(" when trying to play track"));
  //    } else {
  //
  //      Serial.println(F("Playing:"));


  //we can get track info by using the following functions and arguments
  //the functions will extract the requested information, and put it in the array we pass in
  //      MP3player.trackTitle((char*)&title);
  //      MP3player.trackArtist((char*)&artist);
  //      MP3player.trackAlbum((char*)&album);

  //      print out the arrays of track information
  //      Serial.write((byte*)&title, 30);
  //      Serial.println();
  //      Serial.print(F("by:  "));
  //      Serial.write((byte*)&artist, 30);
  //      Serial.println();
  //      Serial.print(F("Album:  "));
  //      Serial.write((byte*)&album, 30);
  //      Serial.println();

  //if +/- to change volume
  //} else if ((key_command == '-') || (key_command == '+')) {
  //    union twobyte mp3_vol; // create key_command existing variable that can be both word and double byte of left and right.
  //    mp3_vol.word = MP3player.getVolume(); // returns a double uint8_t of Left and Right packed into int16_t
  //
  //    if (key_command == '-') { // note dB is negative
  //      // assume equal balance and use byte[1] for math
  //      if (mp3_vol.byte[1] >= 254) { // range check
  //        mp3_vol.byte[1] = 254;
  //      } else {
  //        mp3_vol.byte[1] += 2; // keep it simpler with whole dB's
  //      }
  //    } else {
  //      if (mp3_vol.byte[1] <= 2) { // range check
  //        mp3_vol.byte[1] = 2;
  //      } else {
  //        mp3_vol.byte[1] -= 2;
  //      }
  //    }
  //    // push byte[1] into both left and right assuming equal balance.
  //    MP3player.setVolume(mp3_vol.byte[1], mp3_vol.byte[1]); // commit new volume
  //    Serial.print(F("Volume changed to -"));
  //    Serial.print(mp3_vol.byte[1] >> 1, 1);
  //    Serial.println(F("[dB]"));
  //  }

  // print prompt after key stroke has been processed.
  Serial.print(F("Time since last command: "));
  Serial.println((float) (millis() -  millis_prv) / 1000, 2);
  millis_prv = millis();
  Serial.print(F("Enter s,1-9,+,-,>,<,f,F,d,i,p,t,S,b"));
#if !defined(__AVR_ATmega32U4__)
  Serial.print(F(",m,e,r,R,g,k,O,o,D,V,B,C,T,E,M:"));
#endif
  Serial.println(F(",l,h :"));
}

//------------------------------------------------------------------------------






