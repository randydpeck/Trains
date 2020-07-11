// Rev: 01-29-2019 by Randy
// SD_Card_Audio will play songs on an Arduino using an SD card.  Similar to using a WAV Trigger board.
// 1. Convert music to WAV files per specs below, using link below.
// 2. Copy music onto an SD card (8.3-style filenames).
//    IMPORTANT: Filenames must be "##.wav" i.e. "03.wav" (and not "3.wav")
// 3. Store the number of song as an ASCII string i.e. "12" in the count.txt file on the SD card (write with Notepad app)

// From this article (similar to many): https://www.instructables.com/id/Audio-Player-Using-Arduino-With-Micro-SD-Card/
// Utilizes standard Arduino SD library, SD card and output device (Speaker, Headphones, Amplifier, etc)
// WAV files, 8-bit, 8-32khz Sample Rate, mono.
// Supported devices: Arduino Uno, Nano, Mega, etc.
// Documentation is available on the Wiki: https://github.com/TMRh20/TMRpcm/wiki
// See the wiki https://github.com/TMRh20/TMRpcm/wiki/Advanced-Features  
// Convert audio files here: https://audio.online-convert.com/convert-to-wav or use Audacity.
//   8 bit, 16000 Hz (or 32000, 22050, or 11025), mono, PCM unsigned 8-bit
// In Audacity:
//   Tracks, Mix, Mix Stereo down to mono
//   Tracks, Resample, Sample Rate 16000
//   IMPORTANT: "Project Rate" at bottom left of Audacity window must be set to 16000 before saving (even though resampled to 16000)
//   File, Export, Export Audio.  "Other uncompressed files", WAV, Unsigned 8-bit PCM.

// Mega Ground to speaker out common (to headphone jack Sleeve)
// Mega pin 46 to speaker out hot (to headphone jack Tip and Ring for mono out to stereo speakers)
// Mega pins 50-53, 5v, and Ground to SD card adapter
//   Pin 50 to MISO
//   Pin 51 to MOSI
//   Pin 52 to SCK
//   Pin 53 to SS/CS
// For testing, ground NEXT_TRACK_PIN to skip to the next track before current song ends.
// Note that volume level 5 is max without distortion.

// Added new feature 01/29/19 to (create if doesn't yet exist, and) open the file "lastsong.txt" on the SD card, and keep it
// updated with the number (in string format) of the song that was most recently completed.  So when the power is turned off
// during playback, the system will start with the following song (and not the first song) the next time it's powered up.
// This prevents starting with the first track every time the system is turned on.

#include <SD.h>                      // need to include the SD library
#define SD_ChipSelectPin 53  //hardware SS pin 53 on Mega2560.
#include <TMRpcm.h>
#include <SPI.h>

TMRpcm audio;

File myFile;  // Create an SD card file-type object called myFile, which will hold a count of the number of songs on the SD card
int numSongs = 0;  // Will hold the count from count.txt file on SD card, i.e. 5 songs would be "5"
const byte NEXT_TRACK_PIN = 2;  // Ground this pin to skip to the next track

// **************************************************************************

// DEFINE THE MAXIMUM NUMBER AND NAMES OF ALL SONGS TO BE PLAYED ON THE SD CARD HERE:
const int MAXSONGS = 50;  // If we want more than 99, we'll need to go to a longer filename, or less clumsy code
char filename[MAXSONGS][13] = 
  {
    "01.wav", "02.wav", "03.wav", "04.wav", "05.wav", "06.wav", "07.wav", "08.wav", "09.wav", "10.wav",
    "11.wav", "12.wav", "13.wav", "14.wav", "15.wav", "16.wav", "17.wav", "18.wav", "19.wav", "20.wav",
    "21.wav", "22.wav", "23.wav", "24.wav", "25.wav", "26.wav", "27.wav", "28.wav", "29.wav", "30.wav",
    "31.wav", "32.wav", "33.wav", "34.wav", "35.wav", "36.wav", "37.wav", "38.wav", "39.wav", "40.wav",
    "41.wav", "42.wav", "43.wav", "44.wav", "45.wav", "46.wav", "47.wav", "48.wav", "49.wav", "50.wav"
  };

// **************************************************************************

void setup() {

  Serial.begin(9600);
  if(!SD.begin(SD_ChipSelectPin)) {
    Serial.println("SD fail");
    return;
  }

  // Figure out how many songs are on this card.  Will be read as an ASCII number i.e. "3", "15", etc.
  myFile = SD.open("count.txt");
  if (myFile) {
    numSongs = myFile.parseInt();  // Read ASCII chars in count.txt and convert to an integer.  One song would be "1" <CR/LF>
    myFile.close();
  } else {
    Serial.println("Error opening count.txt");
    while (true) { }
  }
  Serial.print("Number of songs: "); Serial.println(numSongs);

  // SD card data file "lastsong.txt" holds number of the song (in string format) last completed, i.e. "7" for the 7th song.
  // The first time this is called, the file may not exist on the SD card, so if it doesn't yet exist, create it and initialize.
  myFile = SD.open("lastsong.txt");
  if (!myFile) {
    Serial.println("lastsong.txt does not yet exist.");
    myFile = SD.open("lastsong.txt", FILE_WRITE);
    myFile.seek(0);  // Set pointer to beginnnig of file
    myFile.println(numSongs);  // Last song "played" was last song, to force start at first song
    myFile.close();
  } else {
    myFile.close();
    Serial.println("lastsong.txt already exists.");
  }
  
  audio.speakerPin = 46;     // set to 5,6,11 or 46 for Mega, 9 for Uno, Nano, etc.
  audio.disable();           // disables the timer on output pin and stops the music
  audio.stopPlayback();      // stops the music, but leaves the timer running
  audio.isPlaying();         // returns 1 if music playing, 0 if not
  audio.pause();             // pauses/unpauses playback
  audio.quality(1);          // 0 or 1. Set 1 for 2x oversampling
  audio.volume(0);           // 1(up) or 0(down) to control volume
  audio.setVolume(5);        // 0 to 7. Set volume level
  audio.loop(0);             // 0 or 1. Can be changed during playback for full control of looping. 
  //audio.play("filename");    // plays a file
  //audio.play("filename",30); // plays a file starting at 30 seconds into the track
  pinMode(NEXT_TRACK_PIN, INPUT_PULLUP);  // Ground this pin during playback to skip to next track (for testing purposes)
}

void loop() {

  int s = 0;  // Will hold the filename[] array index number of song to play.  I.e s = 0 for song no. 1.
  
  while (true) {
    // Get the number (string format) of the last song completed, and increment MOD numSongs
    myFile = SD.open("lastsong.txt");
    s = (myFile.parseInt() - 1);  // s will now be in the range 0..numSongs - 1)
    myFile.close();
    Serial.print("Last song completed was song number: "); Serial.println(s + 1);
    s = ((s + 1) % numSongs);  // I.e. 0..numSongs - 1
    Serial.print("So we will now play song number: "); Serial.println(s + 1);
    audio.setVolume(0);       // Avoids a loud click when starts playing
    audio.play(filename[s]);
    delay(100);               // Avoids click
    audio.setVolume(5);
    while ((audio.isPlaying()) && (digitalRead(NEXT_TRACK_PIN) != LOW)) { }  // Wait until song ends, or test pin gets grounded
    audio.stopPlayback();
    Serial.println("Song ended.");
    // Now write (s + 1) (since not zero offset) to the lastsong.txt data file, as the last song that was completed.
    Serial.print("Writing completed song offset: "); Serial.println(s + 1);  // Song number i.e. 1..50, not offset 0..49
    myFile = SD.open("lastsong.txt", FILE_WRITE);
    myFile.seek(0);
    myFile.println(s + 1);  // i.e. 1..50, not 0..49
    myFile.close();
    delay(1000);
  }
}
