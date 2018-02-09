/*************************************************************************************************************
 * 
 *  Quakophon
 *  
 *  2018 - 02 - 09
 *  
 *  Teensy - based audio recording system 
 *  for field monitoring of anurans
 * 
 *  (c) Frank DD4WH & Raffael Ernst
 * 
 * 
 * 
 * 
 * DONE:
 * - recording to SD files as stereo WAV-files with header --> immediately readable with Audacity & VLC, but not with Windoof Media Player
 * - Real time clock RTC runs
 * - WAV file name contains time of recording (8.3 file names)
 * - WAV files are stored in folders, folder name = year month day
 * - WAV file has correct time stamp (standard UNIX format)
 * - 
 * 
 * 
 * 
 * 
 * 
 * TODO: 
 * 
 * add zeros in front of day/minute etc., if they are lower than 10
 * playback WAVs by Teensy?
 * serial AND button control of REC, STOP, PLAY
 * automate make directory for each day
 * store files automatically under that directory
 * trigger for recording start taken from the peak objects, if trigger is above threshold --> record for 2 seconds
 * timer for automatic recording schemes
 * 
 * serial menu system to: 
 * - enter exact time
 * - enter trigger value for recordings
 * - or enter time frame for permanent recording
 * - or enter time frame with 5 minutes recording, 10 min no recording etc. 
 * - enter location name for WAV files
 * - 
 * 
 * 
 * 
 * code snippets taken from:
 * https://gist.github.com/JarvusChen
 * Paul Stoffregen Audio lib - https://github.com/PaulStoffregen/Audio/tree/master/examples/Recorder
 * Thank you all for your good work and for sharing your code !
 * Go Open source ! 
 **************************************************************************************************************
***********************************************************************************************************************************

   GNU GPL LICENSE v3

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>

 ************************************************************************************************************************************/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Time.h>
#include <TimeLib.h>

// crackling noise is present when using the Teensy 3.5 SD card slot
//#define USE_TEENSY_SD
// crackling noise much rarer when using the audio board SD slot

// Real Time Clock
//
time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}

AudioInputI2S            i2s2;           
AudioAnalyzePeak         peak2;          
AudioRecordQueue         queue1;         
AudioAnalyzePeak         peak1;          
//AudioPlaySdRaw           playRaw1;     
AudioPlaySdWav           playWAV; 
AudioRecordQueue         queue2;         
AudioOutputI2S           i2s1;           
AudioConnection          patchCord1(i2s2, 0, queue1, 0);
AudioConnection          patchCord2(i2s2, 0, peak1, 0);
AudioConnection          patchCord3(i2s2, 1, queue2, 0);
AudioConnection          patchCord4(i2s2, 1, peak2, 0);
AudioConnection          patchCord5(playWAV, 0, i2s1, 0);
AudioConnection          patchCord6(playWAV, 1, i2s1, 1);

AudioControlSGTL5000     sgtl5000_1;     


// which input on the audio shield will be used?
//const int myInput = AUDIO_INPUT_LINEIN;
const int myInput = AUDIO_INPUT_MIC;


#ifdef USE_TEENSY_SD
  // Use these with the Teensy 3.5 & 3.6 SD card
  #define SDCARD_CS_PIN    BUILTIN_SDCARD
  #define SDCARD_MOSI_PIN  11  // not actually used
  #define SDCARD_SCK_PIN   13  // not actually used
#else
  // Use these with the Teensy Audio Shield
  #define SDCARD_CS_PIN    10
  #define SDCARD_MOSI_PIN  7
  #define SDCARD_SCK_PIN   14
#endif

// Remember which mode we're doing
int mode = 0;  // 0=stopped, 1=recording, 2=playing

const char* const Days[7] = {"Saturday", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"};

unsigned long ChunkSize = 0L;
unsigned long Subchunk1Size = 16;
unsigned int AudioFormat = 1;
unsigned int numChannels = 2;
unsigned long sampleRate = 44100;
unsigned int bitsPerSample = 16;
unsigned long byteRate = sampleRate*numChannels*(bitsPerSample/8);// samplerate x channels x (bitspersample / 8)
unsigned int blockAlign = numChannels*bitsPerSample/8;
unsigned long Subchunk2Size = 0L;
unsigned long recByteSaved = 0L;
unsigned long NumSamples = 0L;
byte byte1, byte2, byte3, byte4;

char directory[24] = "";
char location[12] = "";

char year_p[4];
char month_p[2];
char day_p[2];
char hour_p[2];
char minute_p[2];
char second_p[2];

// The file where data is recorded
File frec;

void setup() {
  // record queue uses this memory to buffer incoming audio.
//  AudioMemory(120); // 60
  AudioMemory(192); // maximum allowed // 60

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  //sgtl5000_1.adcHighPassFilterDisable();
  sgtl5000_1.micGain(50); // can be 0 to 63, 50 seems a reasonable choice, 60 distorts for medium-sized audio signals
  sgtl5000_1.volume(0.65);



  // Initialize the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) 
  {
    // stop here if no SD card, but print a message
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

      setTime (8, 17, 42, 13, 2, 2018);      
      Teensy3Clock.set(now()); 

      // convert int to char
      sprintf(year_p, "%d", year());
      sprintf(month_p, "%d", month());
      sprintf(day_p, "%d", day());
      // concatenate chars
      strcat(directory, location);
      strcat(directory, year_p);
      strcat(directory, month_p);
      strcat(directory, day_p);
          
      Serial.print("Directory ");
      Serial.print(directory);
      Serial.print(" created!");
      // this command is expected to alter the directory properties to include the recent date & time as stamp
      SdFile::dateTimeCallback(dateTime);
      SD.mkdir(directory);
} // END OF SETUP



void loop() {
  // send data only when you receive data:
  if (Serial.available() > 0) {
    // read the incoming byte:
    byte incomingByte = Serial.read();
    // Respond to button presses
    if ( incomingByte == '1' ) {
      Serial.println("Record Button Press");
      if (mode == 2) stopPlaying();
      if (mode == 0) startRecording();
    }
    if ( incomingByte == '2' ) {
      Serial.println("Stop Button Press");
      if (mode == 1) stopRecording();
      if (mode == 2) stopPlaying();
      displayClock();
    }
    if ( incomingByte == '3' ) {
      Serial.println("Play Button Press");
      if (mode == 1) stopRecording();
      if (mode == 0) startPlaying();   
    }
  }
  if (mode == 1) {
    continueRecording();
  }

}
void startRecording() {
  Serial.println("StartRecording");
  // set up file name from
  // location year month day hour minute second
//  String NAME = "REC" + String(year())  + String(month()) + String(day()) + String(hour()) + String(minute()) + String(second()) + ".WAV";
  //String NAME = directory + String("/") + String(hour())  + String(minute()) + String(second())+ String(".WAV");
  char fi[32] = "";
  // convert int to char
  sprintf(hour_p, "%d", hour());
  sprintf(minute_p, "%d", minute());
  sprintf(second_p, "%d", second());
  // concatenate chars
  strcat(fi, directory);
  strcat(fi, "/");
  strcat(fi, hour_p);
  strcat(fi, minute_p);
  strcat(fi, second_p);
  strcat(fi, ".WAV");

  Serial.println(fi);
   if (SD.exists(fi)) {
    // The SD library writes new data to the end of the
    // file, so to start a new recording, the old file
    // must be deleted before new data is written.
    SD.remove(fi);
  }  

  // this command is expected to alter the header of the file to include the recent date & time
  SdFile::dateTimeCallback(dateTime);
  // open the file
  frec = SD.open(fi, FILE_WRITE);
  if (frec) {
    Serial.println("File is open");
    queue1.begin();
    queue2.begin();
    mode = 1;
    recByteSaved = 0L;
  }

}

void continueRecording() {
  if (queue1.available() >= 2 && queue2.available() >= 2) 
//  if (queue1.available() >= 10 && queue2.available() >= 10) 
  {
    byte buffer[512]; // data to write
    mxLR(buffer, queue1.readBuffer(), queue2.readBuffer()); // interleave 
    queue1.freeBuffer(); 
    queue2.freeBuffer();  // free buffer
    frec.write(buffer, 512); // write block
    recByteSaved += 512;
  }
}

void stopRecording() {
  Serial.println("StopRecording");
  queue1.end();
  queue2.end();
  // flush buffer
  while (queue1.available() > 0 && queue2.available() > 0) {
    queue1.readBuffer();
    queue1.freeBuffer();
    queue2.readBuffer();
    queue2.freeBuffer();
  }
  writeOutHeader();
  frec.close(); // close file
  mode = 4;
}

void startPlaying() 
{
  Serial.println("startPlaying");
  playWAV.play("RECORD4.WAV");
  mode = 2;
}

void stopPlaying() 
{
  Serial.println("stopPlaying");
  if (mode == 2) playWAV.stop();
  mode = 0;
}

inline void mxLR(byte *dst, const int16_t *srcL, const int16_t *srcR)
{
    byte cnt = 128;
    int16_t *d = (int16_t *)dst;
    const int16_t *l = srcL;
    const int16_t *r = srcR;

    while (cnt--)
    {
      *(d++) = *l++;
      *(d++) = *r++;
    }
}

void writeOutHeader() { // update WAV header with final filesize/datasize

//  NumSamples = (recByteSaved*8)/bitsPerSample/numChannels;
//  Subchunk2Size = NumSamples*numChannels*bitsPerSample/8; // number of samples x number of channels x number of bytes per sample
  Subchunk2Size = recByteSaved;
  ChunkSize = Subchunk2Size + 36;
  frec.seek(0);
  frec.write("RIFF");
  byte1 = ChunkSize & 0xff;
  byte2 = (ChunkSize >> 8) & 0xff;
  byte3 = (ChunkSize >> 16) & 0xff;
  byte4 = (ChunkSize >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  frec.write("WAVE");
  frec.write("fmt ");
  byte1 = Subchunk1Size & 0xff;
  byte2 = (Subchunk1Size >> 8) & 0xff;
  byte3 = (Subchunk1Size >> 16) & 0xff;
  byte4 = (Subchunk1Size >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  byte1 = AudioFormat & 0xff;
  byte2 = (AudioFormat >> 8) & 0xff;
  frec.write(byte1);  frec.write(byte2); 
  byte1 = numChannels & 0xff;
  byte2 = (numChannels >> 8) & 0xff;
  frec.write(byte1);  frec.write(byte2); 
  byte1 = sampleRate & 0xff;
  byte2 = (sampleRate >> 8) & 0xff;
  byte3 = (sampleRate >> 16) & 0xff;
  byte4 = (sampleRate >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  byte1 = byteRate & 0xff;
  byte2 = (byteRate >> 8) & 0xff;
  byte3 = (byteRate >> 16) & 0xff;
  byte4 = (byteRate >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  byte1 = blockAlign & 0xff;
  byte2 = (blockAlign >> 8) & 0xff;
  frec.write(byte1);  frec.write(byte2); 
  byte1 = bitsPerSample & 0xff;
  byte2 = (bitsPerSample >> 8) & 0xff;
  frec.write(byte1);  frec.write(byte2); 
  frec.write("data");
  byte1 = Subchunk2Size & 0xff;
  byte2 = (Subchunk2Size >> 8) & 0xff;
  byte3 = (Subchunk2Size >> 16) & 0xff;
  byte4 = (Subchunk2Size >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  frec.close();
  Serial.println("header written"); 
  Serial.print("Subchunk2: "); 
  Serial.println(Subchunk2Size); 
}

void displayClock() 
{
/*  uint8_t hour10 = hour() / 10 % 10;
  uint8_t hour1 = hour() % 10;
  uint8_t minute10 = minute() / 10 % 10;
  uint8_t minute1 = minute() % 10;
  uint8_t second10 = second() / 10 % 10;
  uint8_t second1 = second() % 10;
*/
  Serial.print(hour());
  Serial.print(" : ");
  Serial.print(minute());
  Serial.print(" : ");
  Serial.println(second());

/*  char string99 [20];
  tft.fillRect(pos_x_date, pos_y_date, 320 - pos_x_date, 20, ILI9341_BLACK); // erase old string
  tft.setTextColor(ILI9341_ORANGE);
  tft.setFont(Arial_12);
  tft.setCursor(pos_x_date, pos_y_date);
  //  Date: %s, %d.%d.20%d P:%d %d", Days[weekday-1], day, month, year
  sprintf(string99, "%s, %02d.%02d.%04d", Days[weekday()], day(), month(), year());
  tft.print(string99);*/
  Serial.print(Days[weekday()]);
  Serial.print(", ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.println(year());
  
} // end function displayDate


// Helper function to put date & time into correct format for file timestamp
static void dateTime(uint16_t *date, uint16_t *time)
    {
        // return date using FAT_DATE macro to format fields
        *date = FAT_DATE(year(), month(), day());
        // return time using FAT_TIME macro to format fields
        *time = FAT_TIME(hour(), minute(), second());
    }
/*
void printDirectory(File dir, int numTabs) {
   while(true) {

     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("tt");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}
*/
