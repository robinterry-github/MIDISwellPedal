#include <MIDI.h>
//#define DEBUG_MSG
//#define DEBUG_A2D

#if defined(DEBUG_MSG) || defined(DEBUG_A2D)
#define DEBUG
#endif

//#define PLAY_CHORDS_TEST
//#define SYSEX_VOLUME_MESSAGE
#define PEDAL_PIN       A0
#define MARGIN_LOW      0x02
#define MARGIN_HIGH     0x20
#define BAUD_RATE       115200
#define MAX_ANALOGUE    0x3FF
#define LAST_VAL        (MAX_ANALOGUE*2)
//#define INVERT_ANALOGUE
#define MAX_MIDI_VOL    127
#define MAX_CHANNEL     16
#define MIN_VOL         0x010
#define MAX_VOL         0x3F0
#define VOL_CONTROL     11

#ifndef DEBUG
MIDI_CREATE_DEFAULT_INSTANCE();
#endif

int curVal = 0, lastVal = LAST_VAL, delta, volume;
bool pedalPresent = false;
bool forceProcess = false;

/* This defaults to maximum volume for start */
#ifdef SYSEX_VOLUME_MESSAGE
byte masterVolMsg[] = {
  0xF0, 0x7F, 0x7F, 0x04, 0x01, MAX_MIDI_VOL, MAX_MIDI_VOL, 0xF7
};
#endif

void sendMaxVolume()
{
#ifdef DEBUG
  Serial.println("Sending maximum volume");
#else
#ifdef SYSEX_VOLUME_MESSAGE
  masterVolMsg[5] = MAX_MIDI_VOL;
  masterVolMsg[6] = MAX_MIDI_VOL;
  MIDI.sendSysEx(8, masterVolMsg, true);
#endif
  for (int i = 1; i <= MAX_CHANNEL; i++)
      MIDI.sendControlChange(VOL_CONTROL, MAX_MIDI_VOL, i);
#endif
}

void setup()
{
#ifdef DEBUG
  Serial.begin(BAUD_RATE);
#else
  MIDI.begin();
  /* Send maximum volume to start with */
  sendMaxVolume();
#endif
}

void loop() 
{
  for (;;) {
    curVal = analogRead(PEDAL_PIN);

    if (curVal == 0) {
      if (pedalPresent) {
        pedalPresent = false;
        /* When pedal removed, send maximum volume */
        sendMaxVolume();
#ifdef DEBUG
        Serial.println("Pedal removed");
#endif
        delay(1000);
        continue;
      }
    } else {
      if (curVal <= MIN_VOL)
        curVal = MIN_VOL;
      else if (curVal >= MAX_VOL)
        curVal = MAX_ANALOGUE;

      if (!pedalPresent) {
#ifdef DEBUG
        Serial.println("Pedal inserted");
#endif
        /* Pedal inserted, so redo the analogue */
        pedalPresent = forceProcess = true;
        delay(1000);
        continue;
      }
    }
#ifdef INVERT_ANALOGUE     
    curVal = map(curVal, 0, MAX_ANALOGUE, MAX_ANALOGUE, 0);
#endif
    if (pedalPresent) {
      delta = abs(curVal-lastVal);
      volume = map(curVal, 0, MAX_ANALOGUE, 0, MAX_MIDI_VOL);
#ifndef DEBUG
#ifdef PLAY_CHORDS_TEST
      MIDI.sendNoteOn(30+(curVal>>4), MAX_MIDI_VOL, 1);
      MIDI.sendNoteOn(34+(curVal>>4), MAX_MIDI_VOL, 2);
      MIDI.sendNoteOn(37+(curVal>>4), MAX_MIDI_VOL, 3);
      delay(20);
      MIDI.sendNoteOn(30+(curVal>>4), 0, 1);
      MIDI.sendNoteOn(34+(curVal>>4), 0, 2);
      MIDI.sendNoteOn(37+(curVal>>4), 0, 3);
      delay(20);
#endif
#endif
      if ((delta >= MARGIN_LOW && delta <= MARGIN_HIGH)
          ||
          /* Force reprocessing of the analogue value */
          forceProcess) {
        forceProcess = false;
#ifdef DEBUG_A2D
        Serial.print("curVal 0x");
        Serial.print(curVal, HEX);
        Serial.print(" lastVal 0x");
        Serial.print(lastVal, HEX);
        Serial.print(" delta 0x");
        Serial.print(delta, HEX);
        Serial.print(" volume ");
        Serial.println(volume);
#endif
#ifdef SYSEX_VOLUME_MESSAGE
        volume = map(curVal, 0, MAX_ANALOGUE, 0, 0x3FFF);
#ifdef DEBUG_MSG
        Serial.println(volume, HEX);
#endif
        masterVolMsg[5] = (byte) (volume & 0x7F);
        masterVolMsg[6] = (byte) ((volume >> 7) & 0x7F);
#ifdef DEBUG
#ifdef DEBUG_MSG
        for (int j = 0; j < 8; j++) {
          Serial.print(masterVolMsg[j], HEX);
          Serial.print(" ");
        }
        Serial.println("");
#endif
#else
        MIDI.sendSysEx(8, masterVolMsg, true);
#endif
#else // SYSEX_VOLUME_MESSAGE
        for (int i = 1; i <= MAX_CHANNEL; i++) {
#ifdef DEBUG
#ifdef DEBUG_MSG
          Serial.println("=====");
          Serial.print("Controller ");
          Serial.print(VOL_CONTROL);
          Serial.print(" channel ");
          Serial.print(i);
          Serial.print(" volume 0x");
          Serial.println(volume, HEX);
          Serial.println("=====");
#endif
#else
          MIDI.sendControlChange(VOL_CONTROL, volume, i);
#endif
        }
#endif // SYSEX_VOLUME_MESSAGE
      }
      lastVal = curVal;
      delay(2);
    }
  }
}
