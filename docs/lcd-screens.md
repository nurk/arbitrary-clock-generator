# LCD Screen Layouts — 20×4 display

Each screen is shown as a 20-column × 4-row grid.
`█` marks the blinking/underline cursor position. Columns are numbered 0–19.

---

## Main Screen

Channel list. Rotary encoder scrolls the `>` selector between CH0–CH2.
Button A = turn selected channel on, B = off, C or rotary push = enter config screen.

```
col:  0         1         2
      01234567890123456789

row0: >CH0:       1.000,00
row1:  CH1:      50.000,00
row2:  CH2:     100.000,00
row3:  A:On B:Off C:Config
```

- Column 0: `>` = selected, ` ` = not selected
- Columns 1–5: channel label (`CH0: `, `CH1: `, `CH2: `)
- Columns 6–19: frequency, **right-aligned in 14 chars** (variable-width format)
- Row 3: static button hint (exactly 20 chars including leading space)
- Blinking cursor sits at column 0 of the selected row

### Frequency field examples (columns 6–19, 14 chars wide)

| centi-Hz value | Displayed string (14 chars)  |
|---|---|
| `0`             | `          0,00` |
| `100`           | `          1,00` |
| `100000`        | `      1.000,00` |
| `5000000`       | `     50.000,00` |
| `10000000`      | `    100.000,00` |
| `1000000000`    | ` 10.000.000,00` |
| `10000000000`   | `100.000.000,00` |

> **Note:** maximum supported frequency is 100 MHz = `10 000 000 000` centi-Hz.
> The formatted string `100.000.000,00` is exactly 14 characters — the field is full.

---

## Output Channel Config Screen

Entered from the main screen by pressing C or the rotary push button.
Rotary encoder changes the frequency by the selected step.
Rotary push cycles through adjustment steps. A, B, or C returns to the main screen.

```
col: 0         1         2
     01234567890123456789

row0: Channel 0
row1: Set:   00.001.000,00
row2: Real:  00.001.000,00
row3: A|B|C: Back
```

- Row 0: `"Channel "` + channel index (0–2)
- Row 1: `"Set:   "` (7 chars) + set frequency in **fixed 13-char padded format**
- Row 2: `"Real:  "` (7 chars) + actual achieved frequency in **fixed 13-char padded format**
- Row 3: static button hint
- Underline cursor on row 1 indicates which digit the encoder will change

### Fixed-width frequency format (rows 1 & 2, always 13 chars)

Format: `NN.NNN.NNN,NN`  
Digit positions within the 13-char string:

```
col in string:  0  1  .  3  4  5  .  7  8  9  ,  11 12
                N  N  .  N  N  N  .  N  N  N  ,  N  N
                ^  ^     ^  ^  ^     ^  ^  ^     ^  ^
               10MHz 1MHz ...                    0.01Hz
```

On screen (row 1), each digit appears at column `string_col + 7` (due to the `"Set:   "` prefix):

| String col | Screen col | Digit value |
|---|---|---|
| 0  | 7  | 10 MHz |
| 1  | 8  | 1 MHz  |
| 3  | 10 | 100 kHz |
| 4  | 11 | 10 kHz  |
| 5  | 12 | 1 kHz   |
| 7  | 14 | 100 Hz  |
| 8  | 15 | 10 Hz   |
| 9  | 16 | 1 Hz    |
| 11 | 18 | 0.1 Hz  |
| 12 | 19 | 0.01 Hz |

### Frequency adjustment steps

Cycled by pressing the rotary push button. There are 10 steps (indices 0–9).

| Index | `col` (string) | Screen col | Delta (centi-Hz) | Equivalent step |
|---|---|---|---|---|
| 0  | 0  |  7 | 1 000 000 000 | 10 MHz   |
| 1  | 1  |  8 | 100 000 000   | 1 MHz    |
| 2  | 3  | 10 | 10 000 000    | 100 kHz  |
| 3  | 4  | 11 | 1 000 000     | 10 kHz   |
| 4  | 5  | 12 | 100 000       | 1 kHz    |
| 5  | 7  | 14 | 10 000        | 100 Hz   |
| 6  | 8  | 15 | 1 000         | 10 Hz    |
| 7  | 9  | 16 | 100           | 1 Hz     |
| 8  | 11 | 18 | 10            | 0.1 Hz   |
| 9  | 12 | 19 | 1             | 0.01 Hz  |

### Padded frequency format examples

| centi-Hz value | Displayed string (13 chars) |
|---|---|
| `0`             | `00.000.000,00` |
| `12345`         | `00.000.123,45` |
| `123456789`     | `00.123.456,89` |
| `100000000`     | `01.000.000,00` |
| `10000000000`   | `99.999.999,99` (capped by `FREQUENCY_MAX`) |
