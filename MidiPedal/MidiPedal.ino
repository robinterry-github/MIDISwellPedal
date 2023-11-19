/* MIDI Pedal controller
 * developed by Robin Terry (C) 2023
 * 
 * Latest change: November 18 2023
 */
#include <MIDI.h>
//#define DEBUG_EXPR
//#define DEBUG_SOST
//#define DEBUG_A2D_A0
//#define DEBUG_A2D_A1

#if defined(DEBUG_EXPR) || defined(DEBUG_SOST) || defined(DEBUG_A2D_A0) || defined(DEBUG_A2D_A1)
#define DEBUG
#endif

//#define PLAY_CHORDS_TEST
//#define SYSEX_VOLUME_MESSAGE
#define EXPR_PEDAL      A0
#define SOST_PEDAL      A1
#define MARGIN_LOW      0x04
#define MARGIN_HIGH     0x40
#define BAUD_RATE       115200
#define MAX_ANALOGUE    0x3FF
#define LAST_VAL        (MAX_ANALOGUE*2)
//#define INVERT_ANALOGUE
#define MIN_MIDI_CC     0x00
#define MAX_MIDI_CC     0x7F
#define MAX_CHANNEL     16
#define MIN_VOL         0x010
#define MAX_VOL         0x3F0
#define CC_VOL_CONTROL  11
#define CC_SOST_CONTROL 66
#define UP_THRESHOLD    0x50
#define DOWN_THRESHOLD  0x30

#ifndef DEBUG
MIDI_CREATE_DEFAULT_INSTANCE();
#endif

class pedal {
  public:
    int curVal, delta, lastOnOff;
    bool present;
    pedal() { curVal = 0, lastVal = LAST_VAL, present = false, lastOnOff = MIN_MIDI_CC; }
    void saveVal() { lastVal = curVal; }
    void getDelta() { delta = abs(curVal-lastVal); }
    int mapRange() { return map(curVal, 0, MAX_ANALOGUE, MIN_MIDI_CC, MAX_MIDI_CC); }
#ifdef INVERT_ANALOGUE
    void invertValue() { curVal = map(curVal, 0, MAX_ANALOGUE, MAX_ANALOGUE, 0); }
#else
    void invertValue() { }
#endif
#ifdef DEBUG
    void print(String s) {
      Serial.print(s);
      
      Serial.print(" curVal ");
      Serial.print(curVal, HEX);
      Serial.print(" lastVal ");
      Serial.println(lastVal, HEX);
    }
#else
    void print(String s) {}
#endif
    bool onOffChanged() { return (abs(curVal-lastVal) >= MARGIN_LOW) ? true:false; }
    bool dampAnalog() { return (delta >= MARGIN_LOW && delta <= MARGIN_HIGH) ? true:false; }
    int onOff() {
      int val = mapRange();

      /* Add hysteresis */
      if (val >= UP_THRESHOLD)
        lastOnOff = MAX_MIDI_CC;
      else if (val <= DOWN_THRESHOLD)
         lastOnOff = MIN_MIDI_CC;

      return lastOnOff;
    }
  private:
    int lastVal;
};

struct pedal expr, sost;
int controller;
bool pedalPresent = false;
bool forceProcess = false;

/* This defaults to maximum volume for start */
#ifdef SYSEX_VOLUME_MESSAGE
byte masterVolMsg[] = {
  0xF0, 0x7F, 0x7F, 0x04, 0x01, MAX_MIDI_CC, MAX_MIDI_CC, 0xF7
};
#endif

void sendMaxVolume()
{
#ifdef DEBUG_EXPR
  Serial.println("Sending maximum volume");
#else
#ifndef DEBUG
#ifdef SYSEX_VOLUME_MESSAGE
  masterVolMsg[5] = MAX_MIDI_CC;
  masterVolMsg[6] = MAX_MIDI_CC;
  MIDI.sendSysEx(8, masterVolMsg, true);
#endif
  for (int i = 1; i <= MAX_CHANNEL; i++)
      MIDI.sendControlChange(CC_VOL_CONTROL, MAX_MIDI_CC, i);
#endif
#endif
}

void sendMaxSost()
{
  for (int i = 1; i <= MAX_CHANNEL; i++)
     MIDI.sendControlChange(CC_SOST_CONTROL, MIN_MIDI_CC, i);
}

void setup()
{
#ifdef DEBUG
  Serial.begin(BAUD_RATE);
#else
  MIDI.begin();
  /* Send maximum volume to start with */
  sendMaxVolume();
  /* Send sostenuto pedal off to start with */
  sendMaxSost();
#endif
}

void loop() 
{
#ifdef DEBUG
  Serial.println("");
  Serial.println("MIDI pedal unit (debug)");
#endif
  for (;;) {
    /* Deal with the expression pedal here */
    expr.curVal = analogRead(EXPR_PEDAL);

    if (expr.curVal == 0) {
      if (expr.present) {
        expr.present = false;
        /* When pedal removed, send maximum volume */
        sendMaxVolume();
#ifdef DEBUG_EXPR
        Serial.println("Pedal removed");
#endif
        delay(1000);
        continue;
      }
    } else {
      if (expr.curVal <= MIN_VOL)
        expr.curVal = MIN_VOL;
      else if (expr.curVal >= MAX_VOL)
        expr.curVal = MAX_ANALOGUE;

      if (!expr.present) {
#ifdef DEBUG_EXPR
        Serial.println("Pedal inserted");
#endif
        /* Pedal inserted, so redo the analogue */
        expr.present = forceProcess = true;
        delay(1000);
        continue;
      }
    }
    expr.invertValue();

    if (expr.present) {
      expr.getDelta();
      controller = expr.mapRange();
#ifndef DEBUG
#ifdef PLAY_CHORDS_TEST
      MIDI.sendNoteOn(30+(expr.curVal>>4), MAX_MIDI_CC, 1);
      MIDI.sendNoteOn(34+(expr.curVal>>4), MAX_MIDI_CC, 2);
      MIDI.sendNoteOn(37+(expr.curVal>>4), MAX_MIDI_CC, 3);
      delay(20);
      MIDI.sendNoteOn(30+(expr.curVal>>4), 0, 1);
      MIDI.sendNoteOn(34+(expr.curVal>>4), 0, 2);
      MIDI.sendNoteOn(37+(expr.curVal>>4), 0, 3);
      delay(20);
#endif
#endif
      /* The dampAnalog() function damps the analogue reading to make sure it
       * doesn't change too quickly - this can happen when the pedal is removed */
      if (expr.dampAnalog() || forceProcess) {
        forceProcess = false;
#ifdef DEBUG_EXPR
        expr.print("expr");
#endif
#ifdef DEBUG_A2D_A0
        Serial.print("curVal 0x");
        Serial.print(expr.curVal, HEX);
        Serial.print(" delta 0x");
        Serial.print(expr.delta, HEX);
        Serial.print(" volume ");
        Serial.println(volume);
#endif
#ifdef SYSEX_VOLUME_MESSAGE
        controller = map(expr.curVal, 0, MAX_ANALOGUE, 0, 0x3FFF);
#ifdef DEBUG_EXPR
        Serial.println(volume, HEX);
#endif
        masterVolMsg[5] = (byte) (controller & 0x7F);
        masterVolMsg[6] = (byte) ((controller >> 7) & 0x7F);
#ifdef DEBUG_EXPR
        for (int j = 0; j < 8; j++) {
          Serial.print(masterVolMsg[j], HEX);
          Serial.print(" ");
        }
        Serial.println("");
#else
#ifndef DEBUG
        MIDI.sendSysEx(8, masterVolMsg, true);
#endif
#endif
#else // SYSEX_VOLUME_MESSAGE
#ifdef DEBUG_EXPR
        Serial.println("=====");
        Serial.print("Controller ");
        Serial.print(CC_VOL_CONTROL);
        Serial.print(" level 0x");
        Serial.println(controller, HEX);
        Serial.println("=====");
#endif
        for (int i = 1; i <= MAX_CHANNEL; i++) {
#ifndef DEBUG
          MIDI.sendControlChange(CC_VOL_CONTROL, controller, i);
#endif
        }
#endif // SYSEX_VOLUME_MESSAGE
        expr.saveVal();
        delay(2);
      }
    }

    /* Deal with the sostenuto pedal here */
    sost.curVal = analogRead(SOST_PEDAL);
    sost.getDelta();
    sost.invertValue();
#ifdef DEBUG_A2D_A1
    Serial.print("curVal 0x");
    Serial.print(sost.curVal, HEX);
    Serial.print(" delta 0x");
    Serial.println(sost.delta, HEX);
#endif
    if (sost.onOffChanged()) {
#ifndef DEBUG_SOST
       sost.print("sost");
#endif
       /* This is an on/off controller with hysteresis */
       controller = sost.onOff();
#ifdef DEBUG_SOST
       Serial.println("=====");
       Serial.print("Controller ");
       Serial.print(CC_SOST_CONTROL);
       Serial.print(" level 0x");
       Serial.println(controller, HEX);
       Serial.println("=====");
#endif
       for (int i = 1; i <= MAX_CHANNEL; i++) {
#ifndef DEBUG
          MIDI.sendControlChange(CC_SOST_CONTROL, controller, i);
#endif
      }
      sost.saveVal();
      delay(2);
    }
  }
}
