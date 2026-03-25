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
- **Rotary encoder** — MegaCoreX `RotaryEncoder`, FOUR3 latch mode

---

## Architecture — Clock Generation

### Overall strategy

| Frequency range  | Source         | Notes                                      |
|------------------|----------------|--------------------------------------------|
| 7.6 Hz – ~4 kHz  | TCB0/1/2       | ISR pin toggle, 16-bit CCMP                |
| ~4 kHz – 160 MHz | Si5351         | I2C, centi-Hz units                        |

The crossover point between TCB and Si5351 is to be determined by testing — the Si5351 degrades at low frequencies so TCBs take over below the crossover.

### TCB clock generation (`setTCBFrequency`)

- **Mode:** `TCB_CNTMODE_INT_gc` — interrupt fires every `CCMP+1` ticks, ISR toggles the output pin via `PORT.OUTTGL` (~3 cycles). True 50% duty cycle.
- **Formula:** `f = clkHz / (2 × (CCMP + 1))`
- **Frequency units:** centi-Hz (0.01 Hz), matching Si5351 `set_freq()` convention. e.g. `100000` = 1000.00 Hz
- **Returns:** actual frequency achieved in centi-Hz
- **Rounding:** round-to-nearest (not truncate) to minimise error

#### Clock source selection (per TCB, independent)

| Range          | CLKSEL        | clkHz   | Max step @ 4 kHz | Min freq  |
|----------------|---------------|---------|-----------------|-----------|
| freqHz > 122   | `CLKDIV1`     | 16 MHz  | ~2.0 Hz         | 122 Hz    |
| freqHz ≤ 122   | `CLKTCA`      | 1 MHz   | ~0.02 Hz @ 100Hz| 7.63 Hz   |

The three TCBs are **fully independent** — different channels can use different CLKSEL values simultaneously without any interference.

### TCA0 role

- TCA0 is used **only** as a fixed clock divider to provide CLKTCA = 1 MHz to the TCBs.
- Configured once in `initTCA()` as: SINGLE mode, NORMAL wgmode, DIV16, PER=0xFFFF.
- **Never reconfigured at runtime.** This keeps CLKTCA stable and predictable.
- TCA0 drives **no output pins**.

### millis() override

- The MegaCoreX core uses TCB2 as its `millis()` timer by default.
- `initRTCMillis()` disables TCB2 and redirects `millis()` to the **RTC PIT** instead.
- RTC PIT: `CYC32` period on INT32K (32768 Hz) → ~976 µs ≈ 1 ms per tick.
- Custom `millis()` uses a double-read retry loop (no `cli()`/`sei()`) to safely read the 32-bit `rtcMillis` volatile.
- Build flag `--allow-multiple-definition` is set in `platformio.ini` to allow the override.

---

## Key decisions & rationale (history)

- **Why not TCA FRQ mode for OUTPUT2 (PC0)?**
  TCA0 WO0 *can* reach PC0 via `PORTMUX_TCA0_PORTC_gc` — this was explored and implemented. However it was abandoned because changing the TCA prescaler at runtime also changes CLKTCA, which affects TCB0/TCB1 precision unpredictably. Using all three TCBs keeps the design symmetric and predictable.

- **Why not PWM8 mode on TCBs?**
  Only 8-bit resolution (256 steps) — too coarse.

- **Why not millis() toggle approach?**
  millis() has 1 ms resolution → maximum 500 Hz. Jitter from loop() delays. Far inferior to hardware timers.

- **Why CLKDIV1 above 122 Hz instead of always CLKTCA?**
  CLKTCA = 1 MHz gives 32 Hz steps at 4 kHz. CLKDIV1 = 16 MHz gives 2 Hz steps — 16× better. The crossover at 122 Hz is the natural hardware boundary (minimum representable frequency on CLKDIV1).

- **Why no external chip for sub-8 Hz?**
  0–7.6 Hz is essentially DC, not a useful clock. Not worth extra hardware.

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

- Frequencies passed in **centi-Hz** (0.01 Hz units): `si5351.set_freq(140000000ULL, SI5351_CLK0)` = 1,400,000.00 Hz
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

Each of the three clock outputs is represented by an `OutputChannel` instance (defined in `src/OutputChannel.h` / `src/OutputChannel.cpp`).

### Constructor

```cpp
OutputChannel(uint8_t selectPin, uint8_t ledPin, TCB_t& tcb, Si5351& si5351, si5351_clock siClock);
```

All members are `const` / references, initialised in the initialiser list in declaration order.

### Public API

| Method | Description |
|--------|-------------|
| `void turnOff() const` | Drives `ledPin` LOW, disables TCB (`CTRLA = 0`), disables Si5351 output. |
| `uint32_t setFrequency(uint32_t frequencyCentiHz) const` | Routes to TCB or Si5351 based on crossover. Returns actual frequency in centi-Hz. |

### Routing logic in `setFrequency`

- `frequencyCentiHz <= 400000` (≤ 4000.00 Hz) → `setTCBFrequency()`
- `frequencyCentiHz > 400000` (> 4000.00 Hz) → `setSiFrequency()`
- Always calls `turnOff()` first, then drives `ledPin` HIGH.

### `setSiFrequency`

- Drives `selectPin` LOW (active-low select).
- Calls `si5351.set_freq(frequencyCentiHz, siClock)` and enables the output.
- Returns `frequencyCentiHz` unchanged (Si5351 provides no feedback).

### `setTCBFrequency`

- Drives `selectPin` HIGH (active-low select, so HIGH = Si5351 deselected).
- Implements dual clock-source selection (see TCB section above).
- Returns the actual frequency achieved in centi-Hz (rounded to nearest).

### Three independent instances (in `main.cpp`)

```cpp
OutputChannel outputChannel0(SELECT0, OUTPUT_LED0, TCB0, si5351, SI5351_CLK0);
OutputChannel outputChannel1(SELECT1, OUTPUT_LED1, TCB1, si5351, SI5351_CLK1);
OutputChannel outputChannel2(SELECT2, OUTPUT_LED2, TCB2, si5351, SI5351_CLK2);
```

ISRs remain in `main.cpp` (must be, as they are hardware-vector functions):

```cpp
ISR(TCB0_INT_vect) { PORTA.OUTTGL = PIN2_bm; TCB0.INTFLAGS = TCB_CAPT_bm; }
ISR(TCB1_INT_vect) { PORTA.OUTTGL = PIN3_bm; TCB1.INTFLAGS = TCB_CAPT_bm; }
ISR(TCB2_INT_vect) { PORTC.OUTTGL = PIN0_bm; TCB2.INTFLAGS = TCB_CAPT_bm; }
```

---

## Libraries

| Library            | Purpose              | URL / package name                        |
|--------------------|----------------------|-------------------------------------------|
| JC_Button          | Debounced buttons    | https://github.com/JChristensen/JC_Button |
| RotaryEncoder      | Encoder              | https://github.com/mathertel/RotaryEncoder|
| hd44780            | LCD                  | https://github.com/duinoWitchery/hd44780  |
| Si5351Arduino      | Si5351 clock chip    | https://github.com/etherkit/Si5351Arduino |
| MegaCoreX          | Arduino core         | framework-arduino-megaavr-megacorex       |

