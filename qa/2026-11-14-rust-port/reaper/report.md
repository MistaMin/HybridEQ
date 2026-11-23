# Plugin QA: CLAP: HybridEQ

| Check | Status | Detail |
|---|---|---|
| loads | PASS | CLAP: HybridEQ (OpenGrid), 50 parameters |
| finite output | PASS | peak -4.1 dBFS |
| not silent | PASS | rms -15.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1062x845, 9.1% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Oversample |  | ok (-7.4 dB) | ok (0.558%) |
| 1 | Oversample Filter | Oversample=1 | ok (-0.3 dB) | ok (0.558%) |
| 2 | 2nd Harmonic |  | ok (-5.6 dB) | ok (0.558%) |
| 3 | 3rd Harmonic |  | ok (-8.8 dB) | ok (0.558%) |
| 4 | 4th Harmonic |  | ok (-10.8 dB) | ok (0.558%) |
| 5 | 5th Harmonic |  | ok (-13.6 dB) | ok (0.558%) |
| 6 | Low Cut Freq |  | ok (-3.7 dB) | ok (0.558%) |
| 7 | Low Cut Q |  | ok (3.1 dB) | ok (0.558%) |
| 8 | Low Cut Slope |  | ok (-8.3 dB) | ok (0.558%) |
| 9 | Low Cut Mode |  | ok (-8.3 dB) | ok (0.558%) |
| 10 | Low Cut Bypass |  | ok (-7.6 dB) | ok (0.558%) |
| 11 | High Cut Freq |  | ok (-4.6 dB) | ok (0.558%) |
| 12 | High Cut Q |  | ok (3.7 dB) | ok (0.558%) |
| 13 | High Cut Slope |  | ok (-21.8 dB) | ok (0.558%) |
| 14 | High Cut Mode |  | ok (-16.2 dB) | ok (0.558%) |
| 15 | High Cut Bypass |  | ok (-14.4 dB) | ok (0.558%) |
| 16 | Low Freq | Low Gain=1 | ok (1.0 dB) | ok (0.558%) |
| 17 | Low Gain |  | ok (0.6 dB) | ok (0.558%) |
| 18 | Low Type | Low Gain=1 | ok (-3.9 dB) | ok (0.558%) |
| 19 | Low Mode | Low Gain=1 | ok (0.9 dB) | ok (0.558%) |
| 20 | Low Band Bypass | random state 1 | CONDITIONAL (-7.4 dB) | ok (0.558%) |
| 21 | Mid 1 Freq | Mid 1 Gain=1 | ok (1.8 dB) | ok (0.558%) |
| 22 | Mid 1 Gain |  | ok (-0.0 dB) | ok (0.558%) |
| 23 | Mid 1 Q | Mid 1 Gain=1 | ok (0.9 dB) | ok (0.558%) |
| 24 | Mid 1 Type | Mid 1 Gain=1 | ok (-0.2 dB) | ok (0.558%) |
| 25 | Mid 1 Mode | Mid 1 Gain=1 | ok (2.3 dB) | ok (0.558%) |
| 26 | Mid 1 Bypass | random state 1 | CONDITIONAL (0.6 dB) | ok (0.558%) |
| 27 | Mid 2 Freq | Mid 2 Gain=1 | ok (2.6 dB) | ok (0.558%) |
| 28 | Mid 2 Gain |  | ok (0.8 dB) | ok (0.558%) |
| 29 | Mid 2 Q | Mid 2 Gain=1 | ok (0.6 dB) | ok (0.558%) |
| 30 | Mid 2 Type | Mid 2 Gain=1 | ok (0.6 dB) | ok (0.558%) |
| 31 | Mid 2 Mode | Mid 2 Gain=1 | ok (2.1 dB) | ok (0.558%) |
| 32 | Mid 2 Bypass | random state 1 | CONDITIONAL (-4.6 dB) | ok (0.558%) |
| 33 | High Freq | High Gain=1 | ok (1.9 dB) | ok (0.558%) |
| 34 | High Gain |  | ok (-1.3 dB) | ok (0.558%) |
| 35 | High Type | High Gain=1 | ok (-5.8 dB) | ok (0.558%) |
| 36 | High Mode | High Gain=1 | ok (-2.1 dB) | ok (0.558%) |
| 37 | High Band Bypass | random state 1 | CONDITIONAL (-29.6 dB) | ok (0.558%) |
| 38 | Preamp Type |  | ok (-6.2 dB) | ok (0.558%) |
| 39 | Preamp Pad |  | ok (-3.4 dB) | ok (0.558%) |
| 40 | Preamp Gain |  | ok (2.0 dB) | ok (0.558%) |
| 41 | Preamp Bypass |  | ok (-6.2 dB) | ok (0.558%) |
| 42 | Preamp Circuit |  | ok (-8.6 dB) | ok (0.558%) |
| 43 | Preamp HQ | Preamp Circuit=1 | ok (-2.8 dB) | ok (0.558%) |
| 44 | Output Gain |  | ok (-0.6 dB) | ok (0.558%) |
| 45 | Auto Gain |  | ok (-18.3 dB) | ok (0.558%) |
| 46 | Delta |  | ok (-3.5 dB) | ok (0.558%) |
| 47 | Bypass |  | ok (-2.4 dB) | ok (0.558%) |
| 48 | Wet |  | ok (-2.4 dB) | ok (0.558%) |
| 49 | Delta |  | ok (-2.4 dB) | ok (0.558%) |
