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
#include <JC_Button.h>
#include <RotaryEncoder.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <si5351.h>

#include <avr/io.h>
#include <avr/interrupt.h>

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
int encoderPosition = 0;

hd44780_I2Cexp lcd(0x27);
Si5351 si5351;

void processInputs() {
    rotaryButton.read();
    buttonA.read();
    buttonB.read();
    buttonC.read();

    const int newEncoderPosition = encoder.getPosition(); // NOLINT(*-narrowing-conversions)
    if (encoderPosition != newEncoderPosition) {
        //const int diff = newEncoderPosition - encoderPosition;
        //lcdPage        = (lcdPage + diff + lcdController.pageCount()) % lcdController.pageCount();
        //lcdController.update(lcdPage, opMode);
    }
    encoderPosition = newEncoderPosition;
}

void encoderTick() {
    encoder.tick();
}

ISR(RTC_PIT_vect) {
    rtcMillis++;
    RTC.PITINTFLAGS = RTC_PI_bm;
}

unsigned long millis() {
    // rtcMillis is 32-bit, written entirely inside an ISR. Read it twice without
    // masking interrupts: if the ISR fires between the two reads the values will
    // differ, so retry. Typically zero retries; at most one on the rare tick boundary.
    unsigned long a, b;
    do {
        a = rtcMillis;
        b = rtcMillis;
    }
    while (a != b);
    return a;
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
//   • f = clkHz / (2 * (CCMP + 1))
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

ISR(TCB2_INT_vect) {
    PORTC.OUTTGL  = PIN0_bm; // toggle OUTPUT2 (PC0)
    TCB2.INTFLAGS = TCB_CAPT_bm;
}

// TCB INT mode — all three outputs (OUTPUT0/PA2, OUTPUT1/PA3, OUTPUT2/PC0) use
// the same function with their respective TCB instance passed by reference.
//
// Architecture:
//   • Si5351  handles everything above the TCB crossover frequency (~4 kHz).
//   • TCB0/1/2 handle everything below it, where Si5351 accuracy degrades.
//
// Clock source: CLKTCA = F_CPU / 16 = 1 MHz (fixed — TCA0 stays in normal
// free-running mode with DIV16 and is never reconfigured at runtime).
//
// Formula:  f = CLKTCA / (2 × (CCMP + 1))
//
// Two clock sources are used to maximise precision across the full range:
//
//   CLKDIV1 = 16 MHz  for freqHz > 122 Hz:
//     At  200 Hz : step ≈   0.003 Hz   (CCMP ≈ 39999)
//     At  500 Hz : step ≈   0.031 Hz   (CCMP ≈ 15999)
//     At 1000 Hz : step ≈   0.125 Hz   (CCMP ≈  7999)
//     At 2000 Hz : step ≈   0.500 Hz   (CCMP ≈  3999)
//     At 4000 Hz : step ≈   2.000 Hz   (CCMP ≈  1999)
//     Minimum frequency: 16 000 000 / (2 × 65536) = 122 Hz
//
//   CLKTCA = 1 MHz  for freqHz ≤ 122 Hz  (TCA0 fixed DIV16):
//     At    8 Hz : step ≈   0.001 Hz   (CCMP ≈ 62499)
//     At   50 Hz : step ≈   0.010 Hz   (CCMP ≈  9999)
//     At  100 Hz : step ≈   0.020 Hz   (CCMP ≈  4999)
//     Minimum frequency: 1 000 000 / (2 × 65536) ≈ 7.63 Hz
//
// Returns the actual frequency achieved in centi-Hz.
uint32_t setTCBFrequency(TCB_t& tcb, const uint32_t freq_cHz) {
    if (freq_cHz == 0) {
        tcb.CTRLA   = 0;
        tcb.INTCTRL = 0;
        return 0;
    }

    const auto freqHz = static_cast<uint32_t>(static_cast<uint64_t>(freq_cHz) / 100ULL);

    // Clock source selection — two ranges, trading minimum frequency for step resolution:
    //
    //   CLKDIV1  (16 MHz): freqHz > 122 Hz
    //     → step ≈ f² / 8 000 000  e.g. ~2 Hz @ 4 kHz, ~0.5 Hz @ 2 kHz
    //     → minimum frequency: 16 000 000 / (2 × 65536) = 122 Hz
    //
    //   CLKTCA   ( 1 MHz): freqHz ≤ 122 Hz  (TCA0 fixed at DIV16)
    //     → step ≈ f² / 500 000   e.g. ~0.02 Hz @ 100 Hz
    //     → minimum frequency: 1 000 000 / (2 × 65536) ≈ 7.63 Hz
    //
    // The crossover at 122 Hz is the natural boundary: it is both the minimum
    // representable frequency on CLKDIV1 and a point where CLKTCA already has
    // sub-0.02 Hz steps, so no precision is lost by switching there.
    //
    // All three TCBs share CLKTCA, but each TCB independently selects CLKDIV1
    // or CLKTCA via its own CLKSEL bits — they do not interfere with each other.
    uint8_t clkSel;
    uint32_t clkHz;
    if (freqHz > 122UL) {
        clkSel = TCB_CLKSEL_CLKDIV1_gc;
        clkHz  = F_CPU;         // 16 MHz — fine steps above 122 Hz
    } else {
        clkSel = TCB_CLKSEL_CLKTCA_gc;
        clkHz  = F_CPU / 16UL; // 1 MHz  — fine steps below 122 Hz
    }

    // Round to nearest representable frequency.
    // half = number of ticks per half-period; CCMP = half - 1.
    const uint32_t half_x2 = clkHz / freqHz;
    const uint32_t half_lo = half_x2 / 2;
    const uint32_t half_hi = (half_x2 + 1) / 2;

    uint32_t half;
    if (half_lo == 0) {
        half = 1;
    } else {
        const uint32_t f_lo_x2   = clkHz / half_lo;
        const uint32_t f_hi_x2   = clkHz / half_hi;
        const uint32_t target_x2 = 2 * freqHz;
        half = (target_x2 - f_lo_x2 <= f_hi_x2 - target_x2) ? half_lo : half_hi;
        if (half > 0x10000) half = 0x10000;
    }

    const auto ccmp         = static_cast<uint16_t>(half - 1);
    const auto actualHz_cHz = static_cast<uint32_t>(
        static_cast<uint64_t>(clkHz) * 100ULL / (2ULL * half));

    tcb.CTRLA   = 0;
    tcb.CTRLB   = TCB_CNTMODE_INT_gc;
    tcb.CCMP    = ccmp;
    tcb.INTCTRL = TCB_CAPT_bm;
    tcb.CTRLA   = TCB_ENABLE_bm | clkSel;

    return actualHz_cHz;
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

void initRTCMillis() {
    // Disable TCB2, which the MegaCoreX core starts in init() as its millis() timer.
    // Stopping TCB2 here frees it completely so it is available for other use.
    TCB2.CTRLA    = 0; // disable & stop TCB2
    TCB2.INTCTRL  = 0; // disable TCB2 interrupt
    TCB2.INTFLAGS = TCB_CAPT_bm; // clear any pending flag

    // Configure the RTC Periodic Interrupt Timer (PIT) on the internal 32 kHz oscillator.
    // CYC32 => 32 cycles @ 32768 Hz = ~976 µs ≈ 1 ms per tick.
    RTC.CLKSEL = RTC_CLKSEL_INT32K_gc;

    while (RTC.STATUS > 0) {
    }

    RTC.PITINTCTRL = RTC_PI_bm;
    RTC.PITCTRLA   = RTC_PERIOD_CYC32_gc | RTC_PITEN_bm;
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
        lcd.print(F("LCD initialized successfully"));
    }

    if (si5351.init(SI5351_CRYSTAL_LOAD_10PF, 0, 0)) {
        Serial2.println(F("SI5351 initialized successfully"));
    } else {
        Serial2.println(F("Error: SI5351 not detected"));
    }
}

void initUserInputs() {
    buttonA.begin();
    buttonB.begin();
    buttonC.begin();
    rotaryButton.begin();

    attachInterrupt(ROTARY_A, encoderTick, CHANGE);
    attachInterrupt(ROTARY_B, encoderTick, CHANGE);
}

void setup() {
    Serial2.begin(115200);

    initPins();
    initRTCMillis();
    initTCA();
    initI2CDevices();
    initUserInputs();
}

void loop() {
    processInputs();

    static uint32_t last = 0;

    if (millis() - last >= 500) {
        last = millis();

        // Frequencies in centi-Hz (0.01 Hz units), matching Si5351 set_freq() convention
        static uint32_t f0 = 50000;  //  500.00 Hz
        static uint32_t f1 = 80000;  //  800.00 Hz
        static uint32_t f2 = 120000; // 1200.00 Hz

        Serial2.print(F("TCB0 (OUTPUT0) set: "));
        Serial2.print(static_cast<double>(f0) / 100.0, 2);
        Serial2.print(F(" Hz, actual: "));
        Serial2.print(static_cast<double>(setTCBFrequency(TCB0, f0)) / 100.0, 2);
        Serial2.println(F(" Hz"));

        Serial2.print(F("TCB1 (OUTPUT1) set: "));
        Serial2.print(static_cast<double>(f1) / 100.0, 2);
        Serial2.print(F(" Hz, actual: "));
        Serial2.print(static_cast<double>(setTCBFrequency(TCB1, f1)) / 100.0, 2);
        Serial2.println(F(" Hz"));

        Serial2.print(F("TCB2 (OUTPUT2) set: "));
        Serial2.print(static_cast<double>(f2) / 100.0, 2);
        Serial2.print(F(" Hz, actual: "));
        Serial2.print(static_cast<double>(setTCBFrequency(TCB2, f2)) / 100.0, 2);
        Serial2.println(F(" Hz"));

        f0 += 10000; // +100.00 Hz
        f1 += 15000; // +150.00 Hz
        f2 += 20000; // +200.00 Hz

        if (f0 > 500000) f0 = 50000;
        if (f1 > 500000) f1 = 80000;
        if (f2 > 500000) f2 = 120000;
    }
}
