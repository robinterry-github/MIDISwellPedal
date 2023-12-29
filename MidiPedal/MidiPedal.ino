/* MIDI pedal controller
 * developed by Robin Terry (C) 2023
 * 
 * Latest change: December 29 2023
 */
#include <MIDI.h>

#define ENABLE_EXPR_PEDAL
#define ENABLE_SOST_PEDAL

/* Enable one or more of these to turn on debug */
//#define DEBUG_EXPR
//#define DEBUG_EXPR_CTRL
//#define DEBUG_SOST
//#define DEBUG_SOST_CTRL
//#define DEBUG_A2D_A0
//#define DEBUG_A2D_A1
//#define PLAY_CHORDS_TEST

#if defined(DEBUG_EXPR)      || defined(DEBUG_SOST) || \
    defined(DEBUG_EXPR_CTRL) || defined(DEBUG_SOST_CTRL) || \
    defined(DEBUG_A2D_A0)    || defined(DEBUG_A2D_A1)
#define DEBUG
#endif

#ifdef ENABLE_EXPR_PEDAL
//#define SYSEX_VOLUME_MESSAGE
#endif

#define EXPR_PEDAL      A0
#define SOST_PEDAL      A1
#define EXPR_MARGIN_LO  0x04
#define EXPR_MARGIN_HI  0x200
#define SOST_MARGIN_LO  0x180
#define SOST_MARGIN_HI  0x300
#define DEBUG_BAUD_RATE 115200
#define MAX_ANALOGUE    0x3FF
#define LAST_VAL        (MAX_ANALOGUE*2)
#define MIN_MIDI_CC     0x00
#define MAX_MIDI_CC     0x7F
#define MAX_MIDI_CHAN   16
#define EXPR_MIN        0x010
#define EXPR_MAX        0x3F0
#define CC_EXPR_CONTROL 11
#define CC_SOST_CONTROL 66
#define UP_THRESHOLD    0x50
#define DOWN_THRESHOLD  0x10

enum {
 CTRL_EXPR,
 CTRL_SOST,
 CTRL_MAX
};

#ifndef DEBUG
MIDI_CREATE_DEFAULT_INSTANCE();
#endif

class pedal {
  public:
    int curVal;
#ifdef DEBUG
    int delta;
#endif
    pedal(int mgnLo, int mgnHi, int limMin = 0, int limMax = MAX_ANALOGUE) :
      curVal(0),
      lastVal(LAST_VAL),
      present(false),
      lastOnOff(MIN_MIDI_CC),
      marginLow(mgnLo),
      marginHigh(mgnHi),
      limitMin(limMin),
      limitMax(limMax) { }
    void readPedal(int channel) {
      curVal = analogRead(channel);
    }
    void saveVal() {
      lastVal = curVal;
    }
    void getDelta() {
      delta = abs(curVal-lastVal);
    }
    int mapRange() {
      return map(curVal, 0, MAX_ANALOGUE, MIN_MIDI_CC, MAX_MIDI_CC);
    }
    void print(String s) {
#ifdef DEBUG
      Serial.print(s);
      Serial.print(" curVal ");
      Serial.print(curVal, HEX);
      Serial.print(" lastVal ");
      Serial.println(lastVal, HEX);
#endif
    }
    bool onOffChanged() {
      return (delta >= marginLow) ? true:false;
    }
    bool dampAnalog() {
      return (delta >= marginLow && delta <= marginHigh) ? true:false;
    }
    int onOff() {
      int val = mapRange();
      /* Add hysteresis */
      if (val >= UP_THRESHOLD)
        lastOnOff = MIN_MIDI_CC;
      else if (val <= DOWN_THRESHOLD)
         lastOnOff = MAX_MIDI_CC;

      return lastOnOff;
    }
    bool isPresent() {
      return present;
    }
    void setPresent(bool p) {
      present = p;
    }
    void limit() {
      if (curVal <= limitMin)
        curVal = limitMin;
      else if (curVal >= limitMax)
        curVal = limitMax;
    }
    bool tooLow() {
      return (curVal < marginLow) ? true:false;
    }
    private:
      int lastVal, lastOnOff, marginLow, marginHigh, limitMin, limitMax;
#ifndef DEBUG
      int delta;
#endif
      bool present;
};

#ifdef ENABLE_EXPR_PEDAL
struct pedal expr(EXPR_MARGIN_LO, EXPR_MARGIN_HI, EXPR_MIN, EXPR_MAX);
#endif
#ifdef ENABLE_SOST_PEDAL
struct pedal sost(SOST_MARGIN_LO, SOST_MARGIN_HI);
#endif
byte controller[CTRL_MAX];
bool pedalPresent = false;
bool forceProcess = false;

/* This defaults to maximum volume for start */
#ifdef SYSEX_VOLUME_MESSAGE
byte masterVolMsg[] = {
  0xF0, 0x7F, 0x7F, 0x04, 0x01, MAX_MIDI_CC, MAX_MIDI_CC, 0xF7
};
#endif

void sendMaxExpr()
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
  for (int i = 1; i <= MAX_MIDI_CHAN; i++)
     MIDI.sendControlChange(CC_EXPR_CONTROL, MAX_MIDI_CC, i);
#endif
#endif
}

void sendSostOff()
{
#ifdef DEBUG_SOST
  Serial.println("sending sostenuto pedal off");
#else
#ifndef DEBUG
  for (int i = 1; i <= MAX_MIDI_CHAN; i++)
     MIDI.sendControlChange(CC_SOST_CONTROL, MIN_MIDI_CC, i);
#endif
#endif
}

void setup()
{
#ifdef DEBUG
  Serial.begin(DEBUG_BAUD_RATE);
#else
  MIDI.begin();
  /* Send maximum expression pedal to start with */
  sendMaxExpr();
  /* Send sostenuto pedal off to start with */
  sendSostOff();
#endif
}

void loop() 
{
#ifdef DEBUG
  Serial.println("");
  Serial.println("MIDI pedal unit (debug)");
#endif
  for (;;) {
    /* Read the A/D converters */
#ifdef ENABLE_EXPR_PEDAL
    expr.readPedal(EXPR_PEDAL);
#endif
#ifdef ENABLE_SOST_PEDAL
    sost.readPedal(SOST_PEDAL);
#endif

#ifdef ENABLE_EXPR_PEDAL
    /* Deal with the expression pedal here */
    if (expr.tooLow()) {
      if (expr.isPresent()) {
        expr.setPresent(false);
        /* When pedal removed, send maximum volume */
        sendMaxExpr();
#ifdef DEBUG_EXPR
        Serial.println("pedal removed");
#endif
        delay(1000);
        continue;
      }
    } else {
      /* Limit the analogue value */
      expr.limit();
      /* Is the pedal present? */
      if (!expr.isPresent()) {
#ifdef DEBUG_EXPR
        Serial.println("pedal inserted");
#endif
        /* Pedal inserted, so redo the analogue */
        expr.setPresent(true);
        forceProcess = true;
        delay(1000);
        continue;
      }
    }
    if (expr.isPresent()) {
      expr.getDelta();
      controller[CTRL_EXPR] = (byte) expr.mapRange();
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
#endif // DEBUG
      /* The dampAnalog() function damps the analogue reading to make sure it
       * doesn't change too quickly - this can happen when the pedal is removed */
      if (expr.dampAnalog() || forceProcess) {
        forceProcess = false;
        expr.print("expr");
#ifdef DEBUG_A2D_A0
        Serial.print("curVal 0x");
        Serial.print(expr.curVal, HEX);
        Serial.print(" delta 0x");
        Serial.println(expr.delta, HEX);
#endif

#ifdef SYSEX_VOLUME_MESSAGE
        controller[CTRL_EXPR] = map(expr.curVal, 0, MAX_ANALOGUE, 0, 0x3FFF);
#ifdef DEBUG_EXPR
        Serial.println(volume, HEX);
#endif
        masterVolMsg[5] = (byte) (controller[CTRL_EXPR] & 0x7F);
        masterVolMsg[6] = (byte) ((controller[CTRL_EXPR] >> 7) & 0x7F);
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
#ifdef DEBUG_EXPR_CTRL
        Serial.print("expr controller ");
        Serial.print(CC_EXPR_CONTROL);
        Serial.print(" value 0x");
        Serial.println(controller[CTRL_EXPR], HEX);
#endif
#ifndef DEBUG
        for (int i = 1; i <= MAX_MIDI_CHAN; i++)
          MIDI.sendControlChange(CC_EXPR_CONTROL, controller[CTRL_EXPR], i);
#endif
#endif // SYSEX_VOLUME_MESSAGE
        delay(2);
      }
      expr.saveVal();
    }
#endif // ENABLE_EXPR_PEDAL

#ifdef ENABLE_SOST_PEDAL
    /* Deal with the sostenuto pedal here */
    sost.getDelta();
#ifdef DEBUG_A2D_A1
    Serial.print("curVal 0x");
    Serial.print(sost.curVal, HEX);
    Serial.print(" delta 0x");
    Serial.println(sost.delta, HEX);
#endif
    if (sost.onOffChanged()) {
       sost.print("sost");
       /* This is an on/off controller with hysteresis */
       controller[CTRL_SOST] = (byte) sost.onOff();
#ifdef DEBUG_SOST_CTRL
       Serial.print("sost controller ");
       Serial.print(CC_SOST_CONTROL);
       Serial.print(" value 0x");
       Serial.println(controller[CTRL_SOST], HEX);
#endif
#ifndef DEBUG
       for (int i = 1; i <= MAX_MIDI_CHAN; i++)
          MIDI.sendControlChange(CC_SOST_CONTROL, controller[CTRL_SOST], i);
#endif
       delay(2);
    }
    sost.saveVal();
#endif // ENABLE_SOST_PEDAL
    delay(10);
  }
}
