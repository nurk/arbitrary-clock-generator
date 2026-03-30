# LCD Screen Layouts — 20×4 display

Each screen is shown as a 20-column × 4-row grid.
`█` marks the cursor/selected item. Columns are numbered 0–19.

---

## Main Screen

Channel list. Rotary encoder scrolls the `>` selector between CH0–CH2.
Button A = turn selected channel on, B = off, C = enter config screen.

```
col: 0         1         2
     01234567890123456789

row0: >CH0:       1.000,00
row1:  CH1:      50.000,00
row2:  CH2:     100.000,00
row3:  A:On B:Off C:Config
```

- Column 0: `>` = selected, ` ` = not selected
- Columns 1–5: channel label (`CH0: `, `CH1: `, `CH2: `)
- Columns 6–19: frequency, right-aligned in 14 chars

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

*(Not yet implemented — `OUTPUT_CHANNEL` case is a TODO.)*

Planned layout:

```
col: 0         1         2
     01234567890123456789

row0: CH0 Config
row1:   100.000.000,00
row2:  [  step: 1.000 ]
row3: A:Back B:--- C:---
```

- Row 1: current frequency, right-aligned in 20 cols
- Row 2: currently selected adjustment step (from `FREQUENCY_ADJUSTMENTS`)
- Rotary encoder adjusts frequency by the selected step delta
- Rotary push / C = back to main screen

### Frequency adjustment steps

| Index | Column highlight | Delta (centi-Hz) | Equivalent step |
|---|---|---|---|
| 0 | 0 | 1 000 000 000 | 10 MHz |
| 1 | 0 | 100 000 000 | 1 MHz |
| 2 | 1 | 10 000 000 | 100 kHz |
| 3 | 3 | 1 000 000 | 10 kHz |
| 4 | 4 | 100 000 | 1 kHz |
| 5 | 5 | 10 000 | 100 Hz |
| 6 | 7 | 1 000 | 10 Hz |
| 7 | 8 | 100 | 1 Hz |
| 8 | 9 | 10 | 0.1 Hz |
| 9 | 11 | 1 | 0.01 Hz |



