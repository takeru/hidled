//
// Header for PWM DAC Synthesizer ver.20130327
//
// Usage:
//    #define PWMDAC_OUTPUT_PIN << Your using pin# 3/9/10/11 >>
//    #include <PWMDAC_Synth.h>
//       :
//       :
//    Your codes
//
#ifndef PWMDAC_Synth_h
#define PWMDAC_Synth_h

#include <Arduino.h>
#include <wiring_private.h>
#define NumberOf(array) (sizeof(array)/sizeof((array)[0]))
#define cbi16(sfr, bit) (_SFR_WORD(sfr) &= ~_BV(bit))
#define sbi16(sfr, bit) (_SFR_WORD(sfr) |= _BV(bit))

#define PWMDAC_SYNTH_POLYPHONY  6
#define F_NOTE_A 440 // Note-A frequency [Hz]

typedef struct _EnvelopeParam {
  unsigned int attack_speed;
  byte decay_time; // 0..15
  unsigned int sustain_level;
  byte release_time; // 0..15
} EnvelopeParam;

// Channel
//
class MidiChannel {
  protected:
    double pitch_rate; // positive value only: low .. 1.0(center) .. high
    int pitch_bend;    // -8192 .. 0(center) .. +8191 (signed 14bit value)
  public:
    byte modulation;  // 0 ... 127 (unsigned 7 bit - MSB only)
    PROGMEM const byte *wavetable;
    EnvelopeParam env_param;
    MidiChannel() {
      modulation = 0;
      pitch_bend = 0;
      pitch_rate = 1.0;
      env_param.attack_speed = 0x8000;
      env_param.decay_time = 7;
      env_param.sustain_level = 0x8000;
      env_param.release_time = 7;
    }
    double getPitchRate() { return pitch_rate; }
    int getPitchBend() { return pitch_bend; }
    void setPitchBend(int bend) {
      // (value=4096) * 1/(12*4096) -> 1/12 octave
      // 12*4096 -> 3*16384 -> 3<<14
      if( pitch_bend != bend )
        pitch_rate = pow( 2, (float)((pitch_bend=bend)>>8) / (float)(3<<6) );
    }
    void controlChange(byte number, byte value) {
      switch(number) {
        case 1: modulation = value; break;
      }
    }
};

// Voice
//
class VoiceStatus;
typedef void (VoiceStatus::*AdsrHandler)(void);
extern PROGMEM const unsigned long pitchtable[];
class VoiceStatus {
  public:
    VoiceStatus() { soundOff(); }
    void soundOff() {
      ADSR_countdown = 0;
      volume8 = 0;
      volume16 = 0;
      phase = dphase = dphase_pitch_bend = dphase_original = 0L;
    }
    byte getChannel() { return channel; }
    void setChannel(byte channel) { this->channel = channel; }
    byte getNote() { return note; }
    void attack(byte);
    void release() { ADSR_countdown = 1; }
    unsigned int getPriority() {
      unsigned int p = ADSR_countdown;
      return (p << 8) + volume8;
    }
    boolean isVoiceOn() { return volume8 != 0; }
    boolean isVoiceOn(byte channel) {
      return isVoiceOn() && this->channel == channel;
    }
    boolean isVoiceOn(byte channel, byte note) {
      return isVoiceOn(channel) && this->note == note;
    }
    boolean isNoteOn() { return ADSR_countdown > 1; }
    boolean isNoteOn(byte channel) {
      return isNoteOn() && this->channel == channel;
    }
    boolean isNoteOn(byte channel, byte note) {
      return isNoteOn(channel) && this->note == note;
    }
    unsigned int nextPulseWidth() {
      return volume8 * pgm_read_byte(
        wavetable + (byte)((phase += dphase) >> 24)
      );
    }
    void updateEnvelopeStatus(int);
    void setPitchRate(double rate) {
      dphase = dphase_pitch_bend = dphase_original * rate;
    }
  protected:
    byte volume8; // 0..255
    byte channel; // 1..16
    byte note;    // 0..127
    unsigned long phase;
    unsigned long dphase; // diff of phase each ISR() call
    unsigned long dphase_pitch_bend;
    unsigned long dphase_original;
    unsigned int volume16;
    void setVolume(unsigned int next_volume16) {
      volume8 = (volume16 = next_volume16) >> 8;
    }
    byte ADSR_countdown;
    PROGMEM const byte *wavetable;
    EnvelopeParam *env_param_p;
    static const AdsrHandler ADSR_handlers[5];
    void tickAttack();
    void tickDecay();
    void tickRelease();
};

class PWMDACSynth {
  protected:
    static MidiChannel channels[16];
    static VoiceStatus voices[PWMDAC_SYNTH_POLYPHONY];
    static VoiceStatus *getVoiceStatus(byte channel, byte note) {
      VoiceStatus *vsp = voices;
      for( byte i=0; i<PWMDAC_SYNTH_POLYPHONY; i++, vsp++ )
        if( vsp->isNoteOn(channel,note) ) return vsp;
      return NULL;
    }
    static VoiceStatus *getLowestPriorityVoiceStatus() {
      unsigned int priority;
      unsigned int lowest_priority = 0xFFFF;
      byte i_lowest_priority = 0;
      for( byte i=0; i<PWMDAC_SYNTH_POLYPHONY; i++ ) {
        if( (priority = voices[i].getPriority()) < lowest_priority ) {
          lowest_priority = priority;
          i_lowest_priority = i;
        }
      }
      return voices + i_lowest_priority;
    }
    static PROGMEM const byte maxVolumeSineWavetable[];
  public:
    PWMDACSynth();
    // Controller
    static void setup(); // must be called from setup() in main program
    static byte nextPulseWidth() { // must be called from ISR() repeatedly
      unsigned int pulse_width = 0;
      VoiceStatus *vsp = voices;
      do {
        pulse_width += vsp->nextPulseWidth();
      } while( ++vsp <= voices + (PWMDAC_SYNTH_POLYPHONY - 1) );
      return pulse_width >> 8;
    }
    static void updateEnvelopeStatus() { // must be called from loop() repeatedly
      static const byte modulation_dphase = 2;
      static byte modulation_phase = 0;
      int offset = pgm_read_byte( maxVolumeSineWavetable
        + (modulation_phase += modulation_dphase)
      ) - 0x7F;
      VoiceStatus *vsp = voices;
      do {
        vsp->updateEnvelopeStatus(offset);
      } while( ++vsp <= voices + (PWMDAC_SYNTH_POLYPHONY - 1) );
    }
    // MIDI lib compatible methods
    static void noteOff(byte channel, byte pitch, byte velocity) {
      VoiceStatus *vsp = getVoiceStatus(channel,pitch);
      if( vsp != NULL ) vsp->release();
    }
    static void noteOn(byte channel, byte pitch, byte velocity) {
      VoiceStatus *vsp = getVoiceStatus(channel,pitch);
      if( vsp == NULL ) {
        vsp = getLowestPriorityVoiceStatus();
        vsp->soundOff();
        vsp->setChannel(channel);
      }
      vsp->attack(pitch);
    }
    static void pitchBend(byte channel, int bend);
    static void controlChange(byte channel, byte number, byte value) {
      getChannel(channel)->controlChange(number, value);
    }
    // MIDI channel index <-> pointer converter
    static MidiChannel *getChannel(char channel) {
      return channels + (channel - 1);
    }
    static char getChannel(MidiChannel *cp) {
      return (cp-channels) + 1;
    }
    // All channel 
    static void setEnvelope(struct _EnvelopeParam ep) {
      for( int i=0; i<NumberOf(channels); i++ )
        channels[i].env_param = ep;
    }
    static void setWave(PROGMEM const byte *wt) {
      for( int i=0; i<NumberOf(channels); i++ )
        channels[i].wavetable = wt;
    }
    // Built-in wavetables
    static PROGMEM const byte sineWavetable[];
    static PROGMEM const byte squareWavetable[];
    static PROGMEM const byte triangleWavetable[];
    static PROGMEM const byte sawtoothWavetable[];
    static PROGMEM const byte shepardToneSineWavetable[];
    //
    // Utility
    static byte musicalMod12(char);
    static byte musicalMod7(char);
    static byte log2(unsigned int);
    static int musicalConstrain12(int note, int min_note, int max_note) {
      if( max_note < note ) {
        note = max_note - musicalMod12(max_note - note);
      }
      else if( min_note > note ) {
        note = min_note + musicalMod12(note - min_note);
      }
      return note;
    }
};

extern PWMDACSynth PWM_SYNTH;

//
// PWM output pin:
//
//   Reserved by wiring.c (Arduino core):
//     6: OC0A TIMER0
//     5: OC0B TIMER0
//
//   Available pins:
//     9: OC1A TIMER1
//    10: OC1B TIMER1 - maybe used as SS for SPI
//    11: OC2A TIMER2 - maybe used as MOSI for SPI
//     3: OC2B TIMER2 - maybe used as INT1
//
#ifdef PWMDAC_OUTPUT_PIN
//
// Timer1
#if PWMDAC_OUTPUT_PIN == 9 || PWMDAC_OUTPUT_PIN == 10
void PWMDACSynth::setup() {
  pinMode(PWMDAC_OUTPUT_PIN,OUTPUT);
  // No prescaling
  sbi (TCCR1B, CS10);
  cbi (TCCR1B, CS11);
  cbi (TCCR1B, CS12);
  // Phase-correct PWM
  sbi (TCCR1A, WGM10);
  cbi (TCCR1A, WGM11);
  cbi (TCCR1B, WGM12);
  cbi (TCCR1B, WGM13);
#if PWMDAC_OUTPUT_PIN == 9
  // Compare Output Mode
  //    Phase Correct PWM Mode (TCNTn dual-slope operation)
  //      Clear OCnB on Compare Match when up-counting.
  //      Set OCnB on Compare Match when down-counting.
  cbi (TCCR1A, COM1A0);
  sbi (TCCR1A, COM1A1);
#else
  cbi (TCCR1A, COM1B0);
  sbi (TCCR1A, COM1B1);
#endif
  sbi(TIMSK1,TOIE1); // Enable interrupt
}
ISR(TIMER1_OVF_vect) { // Interrupt Service Routine
#if PWMDAC_OUTPUT_PIN == 9
  OCR1A
#else
  OCR1B
#endif
  = PWMDACSynth::nextPulseWidth();
}
#endif // Timer1
//
// Timer2
#if PWMDAC_OUTPUT_PIN == 11 || PWMDAC_OUTPUT_PIN == 3
void PWMDACSynth::setup() {
  pinMode(PWMDAC_OUTPUT_PIN,OUTPUT);
  // No prescaling
  sbi (TCCR2B, CS20);
  cbi (TCCR2B, CS21);
  cbi (TCCR2B, CS22);
  // Phase-correct PWM
  sbi (TCCR2A, WGM20);
  cbi (TCCR2A, WGM21);
  cbi (TCCR2B, WGM22);
#if PWMDAC_OUTPUT_PIN == 11
  cbi (TCCR2A, COM2A0);
  sbi (TCCR2A, COM2A1);
#else
  cbi (TCCR2A, COM2B0);
  sbi (TCCR2A, COM2B1);
#endif
  sbi(TIMSK2,TOIE2); // Enable interrupt
}
ISR(TIMER2_OVF_vect) {
#if PWMDAC_OUTPUT_PIN == 11
  OCR2A
#else
  OCR2B
#endif
  = PWMDACSynth::nextPulseWidth();
}
#endif // Timer2
#endif // PWMDAC_OUTPUT_PIN

#endif // PWMDAC_Synth_h

