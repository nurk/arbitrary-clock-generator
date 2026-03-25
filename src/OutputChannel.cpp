#include "OutputChannel.h"

OutputChannel::OutputChannel(const uint8_t selectPin,
                             const uint8_t ledPin,
                             TCB_t& tcb,
                             Si5351& si5351,
                             const si5351_clock siClock) :
    selectPin(selectPin),
    ledPin(ledPin),
    tcb(tcb),
    si5351(si5351),
    siClock(siClock) {
}

void OutputChannel::turnOff() const {
    digitalWriteFast(ledPin, LOW);
    tcb.CTRLA = 0;
    si5351.output_enable(siClock, 0);
}

uint32_t OutputChannel::setFrequency(const uint32_t frequencyCentiHz) const {
    turnOff();

    digitalWriteFast(ledPin, HIGH);

    if (frequencyCentiHz <= 400000UL) {
        return setTCBFrequency(frequencyCentiHz);
    }
    return setSiFrequency(frequencyCentiHz);
}

uint32_t OutputChannel::setSiFrequency(const uint32_t frequencyCentiHz) const {
    digitalWriteFast(selectPin, LOW);
    si5351.set_freq(frequencyCentiHz, siClock);
    si5351.output_enable(siClock, 1);
    return frequencyCentiHz;
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
uint32_t OutputChannel::setTCBFrequency(const uint32_t frequencyCentiHz) const {
    digitalWriteFast(selectPin, HIGH);

    if (frequencyCentiHz == 0) {
        tcb.CTRLA   = 0;
        tcb.INTCTRL = 0;
        return 0;
    }

    const auto freqHz = static_cast<uint32_t>(static_cast<uint64_t>(frequencyCentiHz) / 100ULL);

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
        clkHz  = F_CPU; // 16 MHz — fine steps above 122 Hz
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
        half                     = (target_x2 - f_lo_x2 <= f_hi_x2 - target_x2) ? half_lo : half_hi;
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
