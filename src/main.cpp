/**
 * Software revision: 1.0
 * Hardware revision: 1.0
 *
 * Libs:
 *
 * JCButton: https://github.com/JChristensen/JC_Button
 * Rotary Encoder: https://github.com/mathertel/RotaryEncoder
 * LCD: https://github.com/duinoWitchery/hd44780
 * Si5351: https://github.com/etherkit/Si5351Arduino
 *
 **/

#include <Arduino.h>
#include <Wire.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include <JC_Button.h>
#include <RotaryEncoder.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <si5351.h>

#include <OutputChannel.h>
#include <UIController.h>

#define OUTPUT0 PIN_PA2
#define OUTPUT1 PIN_PA3
#define OUTPUT2 PIN_PC0
#define SELECT0 PIN_PD0
#define SELECT1 PIN_PD1
#define SELECT2 PIN_PC1
#define OUTPUT_LED0 PIN_PD7
#define OUTPUT_LED1 PIN_PD6
#define OUTPUT_LED2 PIN_PD5
#define BUTTON_A PIN_PD3
#define BUTTON_B PIN_PD2
#define BUTTON_C PIN_PA7
#define ROTARY_PUSH PIN_PA4
#define ROTARY_A PIN_PA6
#define ROTARY_B PIN_PA5

volatile unsigned long rtcMillis = 0;

Button buttonA(BUTTON_A);
Button buttonB(BUTTON_B);
Button buttonC(BUTTON_C);
Button rotaryButton(ROTARY_PUSH);

RotaryEncoder encoder(ROTARY_A, ROTARY_B, RotaryEncoder::LatchMode::FOUR3);

hd44780_I2Cexp lcd(0x27);
Si5351 si5351;

OutputChannel outputChannel0(SELECT0,
                             OUTPUT_LED0,
                             &TCB0,
                             si5351,
                             SI5351_CLK0);

OutputChannel outputChannel1(SELECT1,
                             OUTPUT_LED1,
                             &TCB1,
                             si5351,
                             SI5351_CLK1);

OutputChannel outputChannel2(SELECT2,
                             OUTPUT_LED2,
                             nullptr,
                             si5351,
                             SI5351_CLK2);


OutputChannel* channels[3] = {&outputChannel0, &outputChannel1, &outputChannel2};
UIController uiController(lcd,
                          buttonA,
                          buttonB,
                          buttonC,
                          rotaryButton,
                          encoder,
                          channels);

ISR(PORTA_PORT_vect) {
    if (PORTA.INTFLAGS & (PIN5_bm | PIN6_bm)) {
        encoder.tick();
        PORTA.INTFLAGS = PIN5_bm | PIN6_bm;
    }
}

// TCB INT mode — pin toggled manually in ISR.
//
// Why not PWM8 or FRQ mode:
//   PWM8_gc  — drives WO pin but only 8-bit TOP (256 steps). Very coarse.
//   FRQ_gc   — 16-bit CCMP but is an INPUT CAPTURE mode, not an output mode.
//              The WO pin is not driven at all.
//
// INT mode — TCB fires an interrupt every CCMP+1 ticks. The ISR toggles the
// output pin directly via the PORT OUTTGL register (~3 cycles). This gives:
//   • Full 16-bit CCMP resolution
//   • True 50% duty cycle (equal half-periods)
//   • f = clkHz / (2 × (CCMP + 1))
//
// Clock source: CLKTCA = 1 MHz (TCA0 reconfigured to DIV16 in initTCA).
// ISR overhead at 10 kHz: fires 20 000×/sec, ~800 cycles between calls → <2% CPU.
//
// freq_cHz : centi-Hz (0.01 Hz units), matching Si5351 set_freq() convention.
//            e.g. 100000 = 1000.00 Hz,  1000000 = 10000.00 Hz
//
// Minimum frequency with CLKTCA:  1 000 000 / (2 × 65536) ≈ 7.63 Hz (763 cHz)
// Maximum frequency with CLKDIV1: 16 000 000 / (2 × 1)    = 8 MHz
//
// Note: 8 kHz has inherent −7937 ppm error from any prescaler of a 16 MHz clock
//       because 16 000 000 / 8000 = 2000, but 1 000 000 / 8000 = 125 exactly —
//       wait, 1MHz/8kHz=125 → CCMP=62, actual=1000000/(2*63)=7936.5 Hz. The only
//       exact solution would need a 8 kHz-divisible source clock.

ISR(TCB0_INT_vect) {
    PORTA.OUTTGL  = PIN2_bm; // toggle OUTPUT0 (PA2)
    TCB0.INTFLAGS = TCB_CAPT_bm;
}

ISR(TCB1_INT_vect) {
    PORTA.OUTTGL  = PIN3_bm; // toggle OUTPUT1 (PA3)
    TCB1.INTFLAGS = TCB_CAPT_bm;
}

void initPins() {
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);
    pinMode(ROTARY_PUSH, INPUT_PULLUP);
    pinMode(ROTARY_A, INPUT_PULLUP);
    pinMode(ROTARY_B, INPUT_PULLUP);

    pinMode(SELECT0, OUTPUT);
    pinMode(SELECT1, OUTPUT);
    pinMode(SELECT2, OUTPUT);
    pinMode(OUTPUT_LED0, OUTPUT);
    pinMode(OUTPUT_LED1, OUTPUT);
    pinMode(OUTPUT_LED2, OUTPUT);
    pinMode(OUTPUT0, OUTPUT);
    pinMode(OUTPUT1, OUTPUT);
    pinMode(OUTPUT2, OUTPUT);
    digitalWriteFast(SELECT0, LOW);
    digitalWriteFast(SELECT1, LOW);
    digitalWriteFast(SELECT2, LOW);
    digitalWriteFast(OUTPUT_LED0, LOW);
    digitalWriteFast(OUTPUT_LED1, LOW);
    digitalWriteFast(OUTPUT_LED2, LOW);
    digitalWriteFast(OUTPUT0, LOW);
    digitalWriteFast(OUTPUT1, LOW);
    digitalWriteFast(OUTPUT2, LOW);
}

void initTCA() {
    // Reconfigure TCA0 to SINGLE mode with a fixed DIV16 prescaler (CLKTCA = 1 MHz).
    // TCA is used solely as a clock source for TCB0/TCB1/TCB2 via CLKTCA — it is
    // never reconfigured at runtime. This keeps CLKTCA stable and predictable.
    //
    // At 1 MHz, most sub-5 kHz frequencies are exactly representable as integer
    // CCMP values, giving clean step behaviour across the TCB operating range.
    TCA0.SINGLE.CTRLA = 0; // disable while reconfiguring
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc; // normal free-running mode
    TCA0.SINGLE.PER   = 0xFFFF;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc | TCA_SINGLE_ENABLE_bm;
}

void initI2CDevices() {
    Wire.swap(1);
    Wire.begin();
    Wire.setClock(400000UL);

    if (lcd.begin(20, 4) != 0) {
        Serial2.println(F("Error: LCD not detected"));
    } else {
        lcd.clear();
        Serial2.println(F("LCD initialized successfully"));
        lcd.setCursor(0, 0);
        lcd.print(F("LCD initialized"));
    }

    lcd.setCursor(0, 1);
    if (si5351.init(SI5351_CRYSTAL_LOAD_10PF, 0, -28080)) {
        Serial2.println(F("SI5351 initialized successfully"));
        lcd.print(F("SI5351 initialized"));
    } else {
        Serial2.println(F("Error: SI5351 not detected"));
        lcd.print(F("SI5351 error"));
    }

    outputChannel0.loadFromEEPROM();
    outputChannel1.loadFromEEPROM();
    outputChannel2.loadFromEEPROM();

    outputChannel0.turnOff();
    outputChannel1.turnOff();
    outputChannel2.turnOff();
}

void initUserInputs() {
    buttonA.begin();
    buttonB.begin();
    buttonC.begin();
    rotaryButton.begin();

    // Configure PA5 (ROTARY_B) and PA6 (ROTARY_A) to trigger a port interrupt on both edges.
    // This replaces attachInterrupt() with direct register access — no overhead, no indirection.
    PORTA.PIN5CTRL |= PORT_ISC_BOTHEDGES_gc;
    PORTA.PIN6CTRL |= PORT_ISC_BOTHEDGES_gc;
}

void setup() {
    Serial2.begin(115200);

    initPins();
    initTCA();
    initI2CDevices();
    initUserInputs();
    uiController.updateScreen();
}

void loop() {
    uiController.processInputs();
}
