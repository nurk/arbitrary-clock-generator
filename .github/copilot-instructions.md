# Copilot Session Notes — Arbitrary Clock Generator

## Hardware

- **MCU:** ATmega4808, 28-pin package, 16 MHz external crystal
- **Framework:** Arduino via MegaCoreX (PlatformIO)
- **Board variant:** `28pin-standard`
- **Platform:** `atmelmegaavr`

### Available ports on 28-pin ATmega4808

| Port | Available pins |
|------|---------------|
| PA   | PA0–PA7       |
| PC   | PC0–PC3 only  |
| PD   | PD0–PD7       |
| PF   | PF0, PF1, PF6 (RESET) |

PB and PE are **not bonded** on the 28-pin package.

### Pin assignments

| Signal      | Pin     | Notes                        |
|-------------|---------|------------------------------|
| OUTPUT0     | PIN_PA2 | TCB0 ISR toggle              |
| OUTPUT1     | PIN_PA3 | TCB1 ISR toggle              |
| OUTPUT2     | PIN_PC0 | TCB2 ISR toggle              |
| SELECT0     | PIN_PD0 |                              |
| SELECT1     | PIN_PD1 |                              |
| SELECT2     | PIN_PC1 |                              |
| OUTPUT_LED0 | PIN_PD7 |                              |
| OUTPUT_LED1 | PIN_PD6 |                              |
| OUTPUT_LED2 | PIN_PD5 |                              |
| BUTTON_A    | PIN_PD3 |                              |
| BUTTON_B    | PIN_PD2 |                              |
| BUTTON_C    | PIN_PA7 |                              |
| ROTARY_PUSH | PIN_PA4 |                              |
| ROTARY_A    | PIN_PA6 |                              |
| ROTARY_B    | PIN_PA5 |                              |

### External chips

- **Si5351** — I2C clock generator, Wire on alternate pins (`Wire.swap(1)`), 400 kHz
- **hd44780 LCD** — 20×4, I2C address 0x27
- **Rotary encoder** — MegaCoreX `RotaryEncoder`, FOUR3 latch mode, `getPosition()` returns `long`

---

## Architecture — Clock Generation

### Overall strategy

| Frequency range   | Source   | Notes                               |
|-------------------|----------|-------------------------------------|
| 7.63 Hz – 4 kHz   | TCB0/1/2 | ISR pin toggle, 16-bit CCMP         |
| > 4 kHz – 160 MHz | Si5351   | I2C, centi-Hz units                 |

Crossover is fixed at **400 000 cHz (4000.00 Hz)**. The Si5351 degrades at low frequencies; TCBs cover below this point.

### TCB clock generation (`setTCBFrequency`)

- **Mode:** `TCB_CNTMODE_INT_gc` — interrupt fires every `CCMP+1` ticks, ISR toggles the output pin via `PORT.OUTTGL` (~3 cycles). True 50% duty cycle.
- **Formula:** `f = clkHz / (2 × (CCMP + 1))`
- **Frequency units:** centi-Hz (0.01 Hz), matching Si5351 `set_freq()` convention. e.g. `100000` = 1000.00 Hz
- **Returns:** actual frequency achieved in centi-Hz (rounded to nearest)
- **`TCB_ENABLE_bm` is NOT set** by `setTCBFrequency` — `turnOn()` sets it. This allows programming frequency while the channel is off.
- **Frequency = 0** is handled explicitly: TCB is disabled and interrupt cleared. `turnOn()` also guards against enabling the TCB when frequency is 0.

#### Clock source selection (per TCB, independent)

| Range          | CLKSEL    | clkHz  | Min freq  |
|----------------|-----------|--------|-----------|
| freqHz > 122   | `CLKDIV1` | 16 MHz | 122 Hz    |
| freqHz ≤ 122   | `CLKTCA`  | 1 MHz  | 7.63 Hz   |

The three TCBs are **fully independent** — different channels can use different CLKSEL values simultaneously without interference.

### TCA0 role

- TCA0 is used **only** as a fixed clock divider: CLKTCA = 1 MHz (F_CPU / DIV16).
- Configured once in `initTCA()` as: SINGLE mode, NORMAL wgmode, DIV16, PER=0xFFFF.
- **Never reconfigured at runtime.** This keeps CLKTCA stable and predictable.
- TCA0 drives **no output pins**.

### millis() override

- The MegaCoreX core uses TCB2 as its `millis()` timer by default.
- `initRTCMillis()` disables TCB2 and redirects `millis()` to the **RTC PIT** instead, freeing TCB2 for clock generation.
- RTC PIT: `CYC32` period on INT32K (32768 Hz) → ~976 µs ≈ 1 ms per tick.
- Custom `millis()` uses a double-read retry loop (no `cli()`/`sei()`) to safely read the 32-bit `rtcMillis` volatile.
- Build flag `--allow-multiple-definition` is set in `platformio.ini` to allow the override.

---

## Key decisions & rationale (history)

- **Why not TCA FRQ mode for OUTPUT2 (PC0)?**
  TCA0 WO0 *can* reach PC0 via `PORTMUX_TCA0_PORTC_gc` — explored and implemented, then abandoned. Changing the TCA prescaler at runtime also changes CLKTCA, affecting TCB0/TCB1 precision unpredictably. All three TCBs keeps the design symmetric.

- **Why not PWM8 mode on TCBs?**
  Only 8-bit resolution (256 steps) — too coarse.

- **Why not millis() toggle approach?**
  millis() has 1 ms resolution → maximum 500 Hz. Jitter from loop() delays.

- **Why CLKDIV1 above 122 Hz?**
  CLKTCA = 1 MHz gives 32 Hz steps at 4 kHz. CLKDIV1 = 16 MHz gives 2 Hz steps — 16× better. The crossover at 122 Hz is the natural hardware boundary.

- **Why no external chip for sub-8 Hz?**
  0–7.6 Hz is essentially DC. Not worth extra hardware.

---

## Precision summary (TCB, CLKDIV1 = 16 MHz, above 122 Hz)

| Frequency | Step size |
|-----------|-----------|
| 200 Hz    | ~0.003 Hz |
| 500 Hz    | ~0.031 Hz |
| 1000 Hz   | ~0.125 Hz |
| 2000 Hz   | ~0.500 Hz |
| 4000 Hz   | ~2.000 Hz |

## Precision summary (TCB, CLKTCA = 1 MHz, below 122 Hz)

| Frequency | Step size  |
|-----------|------------|
| 8 Hz      | ~0.001 Hz  |
| 50 Hz     | ~0.010 Hz  |
| 100 Hz    | ~0.020 Hz  |

---

## Si5351 notes

- Frequencies passed in **centi-Hz** (0.01 Hz units): `si5351.set_freq(100000000ULL, SI5351_CLK0)` = 1,000,000.00 Hz
- Outputs 50% duty cycle square waves
- Crystal load: `SI5351_CRYSTAL_LOAD_10PF`, auto-detect crystal frequency (0)
- Accurate above ~4 kHz; degrades at lower frequencies (exact crossover TBD by testing)

---

## platformio.ini highlights

```ini
platform  = atmelmegaavr
board     = ATmega4808
framework = arduino
board_build.variant   = 28pin-standard
board_build.f_cpu     = 16000000L
board_hardware.oscillator = external
build_flags =
    -Wl,-u,vfprintf -lprintf_flt -lm
    -Wl,--allow-multiple-definition   ; needed for millis() override
upload_protocol = serialupdi
upload_speed    = 230400
upload_flags    = -xrtsdtr=high
monitor_speed   = 115200
```

---

## Code architecture — `OutputChannel` class

Each of the three clock outputs is represented by an `OutputChannel` instance (`src/OutputChannel.h` / `src/OutputChannel.cpp`).

### Constructor

```cpp
OutputChannel(uint8_t selectPin, uint8_t ledPin, TCB_t& tcb, Si5351& si5351, si5351_clock siClock);
```

### Public API

| Method | Description |
|--------|-------------|
| `void turnOff()` | Drives `ledPin` LOW, disables TCB, disables Si5351 output. Sets `isOn_ = false`. |
| `void turnOn()` | Drives `ledPin` HIGH. Enables TCB (if freq ≤ crossover and freq ≠ 0) or Si5351. Sets `isOn_ = true`. |
| `uint64_t setFrequency(uint64_t frequencyCentiHz)` | Routes to TCB or Si5351. Saves `wasOn`, calls `turnOff()`, programs hardware, calls `turnOn()` only if `wasOn`. Returns actual frequency achieved. |
| `uint64_t getActualFrequency() const` | Returns the hardware-achieved frequency (may differ from set due to TCB rounding). |
| `uint64_t getSetFrequency() const` | Returns the user-requested frequency (unmodified). |

### Frequency types — all `uint64_t`

All public frequency values are `uint64_t` centi-Hz. The Si5351 library also uses `uint64_t`. `int64_t` is only used transiently in `UIController` for signed encoder delta arithmetic.

### Routing logic in `setFrequency`

- `frequencyCentiHz <= 400000` (≤ 4000.00 Hz) → `setTCBFrequency()` — drives `selectPin` HIGH
- `frequencyCentiHz > 400000` (> 4000.00 Hz) → `setSiFrequency()` — drives `selectPin` LOW
- SELECT pin is **active-low** for the Si5351 mux.

### Three independent instances (in `main.cpp`)

```cpp
OutputChannel outputChannel0(SELECT0, OUTPUT_LED0, TCB0, si5351, SI5351_CLK0);
OutputChannel outputChannel1(SELECT1, OUTPUT_LED1, TCB1, si5351, SI5351_CLK1);
OutputChannel outputChannel2(SELECT2, OUTPUT_LED2, TCB2, si5351, SI5351_CLK2);
```

ISRs remain in `main.cpp`:

```cpp
ISR(TCB0_INT_vect) { PORTA.OUTTGL = PIN2_bm; TCB0.INTFLAGS = TCB_CAPT_bm; }
ISR(TCB1_INT_vect) { PORTA.OUTTGL = PIN3_bm; TCB1.INTFLAGS = TCB_CAPT_bm; }
ISR(TCB2_INT_vect) { PORTC.OUTTGL = PIN0_bm; TCB2.INTFLAGS = TCB_CAPT_bm; }
```

---

## Code architecture — `UIController` class

Defined in `src/UIController.h` / `src/UIController.cpp`. Owns all UI logic.

### Constructor

```cpp
UIController(hd44780_I2Cexp& lcd, Button& buttonA, Button& buttonB, Button& buttonC,
             Button& rotaryButton, RotaryEncoder& rotaryEncoder, OutputChannel* outputChannels[3]);
```

Stores pointers to the three `OutputChannel` instances in `outputChannels_[3]`.

### Screens

| Screen | Enum | Description |
|--------|------|-------------|
| Main   | `MAIN` | Shows all 3 channels with actual frequency. Encoder scrolls channel selector. |
| Config | `OUTPUT_CHANNEL` | Shows Set and Real frequency for selected channel. Encoder adjusts frequency. |

### Main screen

```
>CH0:       1.000,00
 CH1:      50.000,00
 CH2:     100.000,00
 A:On B:Off C:Config
```

- Frequency is **right-aligned in 14 chars**, variable-width format (e.g. `    100.000,00`)
- Shows `getActualFrequency()` (what the hardware produces)
- Blinking cursor at column 0 of selected row
- A = `turnOn()`, B = `turnOff()`, C or rotary push = enter config screen

### Config screen

```
Channel 0
Set:   00.001.000,00
Real:  00.001.000,00
A|B|C: Back
```

- Row 1: `"Set:   "` (7 chars) + 13-char **fixed-width padded** `getSetFrequency()` (`NN.NNN.NNN,NN`)
- Row 2: `"Real:  "` (7 chars) + 13-char **fixed-width padded** `getActualFrequency()`
- Underline cursor on row 1 at `FREQUENCY_ADJUSTMENTS[index].col + 7` (the +7 accounts for the prefix)
- Rotary push cycles through 10 frequency adjustment steps
- A/B/C returns to main screen

### Frequency adjustment steps (`FREQUENCY_ADJUSTMENTS[10]`)

| Index | `col` | Delta (cHz) | Step    |
|-------|-------|-------------|---------|
| 0     | 0     | 1 000 000 000 | 10 MHz |
| 1     | 1     | 100 000 000   | 1 MHz  |
| 2     | 3     | 10 000 000    | 100 kHz|
| 3     | 4     | 1 000 000     | 10 kHz |
| 4     | 5     | 100 000       | 1 kHz  |
| 5     | 7     | 10 000        | 100 Hz |
| 6     | 8     | 1 000         | 10 Hz  |
| 7     | 9     | 100           | 1 Hz   |
| 8     | 11    | 10            | 0.1 Hz |
| 9     | 12    | 1             | 0.01 Hz|

### Encoder delta arithmetic

```cpp
const int64_t newFreq = static_cast<int64_t>(outputChannel->getSetFrequency())
                      + diff * FREQUENCY_ADJUSTMENTS[index].delta;
outputChannel->setFrequency(
    static_cast<uint64_t>(max(FREQUENCY_MIN, min(FREQUENCY_MAX, newFreq)))
);
```

- Based on `getSetFrequency()` so small steps accumulate correctly regardless of TCB rounding
- `diff` is `long` (matches `RotaryEncoder::getPosition()` return type)
- `delta` is `int64_t` so multiplication is signed — turning left decreases frequency correctly
- Clamped to `[FREQUENCY_MIN=0, FREQUENCY_MAX=9999999999]` before casting back to `uint64_t`

### Frequency constants

```cpp
const int64_t FREQUENCY_MAX = 9999999999LL; // ≈ 99.999 MHz
const int64_t FREQUENCY_MIN = 0LL;
```

`int64_t` so the clamp arithmetic (`max`/`min`) works correctly with signed `newFreq`.

---

## Libraries

| Library       | Purpose           | URL / package name                         |
|---------------|-------------------|--------------------------------------------|
| JC_Button     | Debounced buttons | https://github.com/JChristensen/JC_Button  |
| RotaryEncoder | Encoder           | https://github.com/mathertel/RotaryEncoder |
| hd44780       | LCD               | https://github.com/duinoWitchery/hd44780   |
| Si5351Arduino | Si5351 clock chip | https://github.com/etherkit/Si5351Arduino  |
| MegaCoreX     | Arduino core      | framework-arduino-megaavr-megacorex        |

