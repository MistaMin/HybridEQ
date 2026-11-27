# Plugin QA: VST3: HybridEQ

| Check | Status | Detail |
|---|---|---|
| loads | PASS | VST3: HybridEQ (OpenGrid), 51 parameters |
| finite output | PASS | peak -4.1 dBFS |
| not silent | PASS | rms -15.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1062x845, 8.3% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Oversample |  | ok (-9.1 dB) | ok (3.727%) |
| 1 | Oversample Filter | Oversample=1 | ok (-5.1 dB) | ok (2.012%) |
| 2 | 2nd Harmonic |  | ok (-0.6 dB) | ok (2.210%) |
| 3 | 3rd Harmonic |  | ok (-8.8 dB) | ok (2.498%) |
| 4 | 4th Harmonic |  | ok (-10.8 dB) | ok (1.065%) |
| 5 | 5th Harmonic |  | ok (-13.6 dB) | ok (0.962%) |
| 6 | Low Cut Freq |  | ok (-3.7 dB) | ok (3.586%) |
| 7 | Low Cut Q |  | ok (3.1 dB) | ok (1.500%) |
| 8 | Low Cut Slope |  | ok (-8.3 dB) | ok (0.959%) |
| 9 | Low Cut Mode |  | ok (-8.3 dB) | ok (1.000%) |
| 10 | Low Cut Bypass |  | ok (-7.6 dB) | ok (3.629%) |
| 11 | High Cut Freq |  | ok (-4.6 dB) | ok (4.247%) |
| 12 | High Cut Q |  | ok (3.7 dB) | ok (2.639%) |
| 13 | High Cut Slope |  | ok (-21.8 dB) | ok (1.681%) |
| 14 | High Cut Mode |  | ok (-16.2 dB) | ok (3.137%) |
| 15 | High Cut Bypass |  | ok (-14.4 dB) | ok (3.116%) |
| 16 | Low Freq | Low Gain=1 | ok (1.0 dB) | ok (1.674%) |
| 17 | Low Gain |  | ok (0.6 dB) | ok (4.487%) |
| 18 | Low Type | Low Gain=1 | ok (-3.9 dB) | ok (9.716%) |
| 19 | Low Mode | Low Gain=1 | ok (0.9 dB) | ok (1.817%) |
| 20 | Low Band Bypass | random state 1 | CONDITIONAL (-7.4 dB) | ok (1.654%) |
| 21 | Mid 1 Freq | Mid 1 Gain=1 | ok (1.8 dB) | ok (3.508%) |
| 22 | Mid 1 Gain |  | ok (-0.0 dB) | ok (2.861%) |
| 23 | Mid 1 Q | Mid 1 Gain=1 | ok (0.9 dB) | ok (3.117%) |
| 24 | Mid 1 Type | Mid 1 Gain=1 | ok (-0.2 dB) | ok (9.825%) |
| 25 | Mid 1 Mode | Mid 1 Gain=1 | ok (2.3 dB) | ok (1.648%) |
| 26 | Mid 1 Bypass | random state 1 | CONDITIONAL (0.6 dB) | ok (2.827%) |
| 27 | Mid 2 Freq | Mid 2 Gain=1 | ok (2.6 dB) | ok (2.812%) |
| 28 | Mid 2 Gain |  | ok (0.8 dB) | ok (3.285%) |
| 29 | Mid 2 Q | Mid 2 Gain=1 | ok (0.6 dB) | ok (1.915%) |
| 30 | Mid 2 Type | Mid 2 Gain=1 | ok (0.6 dB) | ok (7.250%) |
| 31 | Mid 2 Mode | Mid 2 Gain=1 | ok (2.1 dB) | ok (3.614%) |
| 32 | Mid 2 Bypass | random state 1 | CONDITIONAL (-4.6 dB) | ok (2.226%) |
| 33 | High Freq | High Gain=1 | ok (1.9 dB) | ok (3.607%) |
| 34 | High Gain |  | ok (-1.3 dB) | ok (4.219%) |
| 35 | High Type | High Gain=1 | ok (-5.8 dB) | ok (9.605%) |
| 36 | High Mode | High Gain=1 | ok (-2.1 dB) | ok (1.650%) |
| 37 | High Band Bypass | random state 1 | CONDITIONAL (-29.6 dB) | ok (3.058%) |
| 38 | Preamp Type |  | ok (-6.2 dB) | ok (5.846%) |
| 39 | Preamp Pad |  | ok (-3.4 dB) | ok (3.578%) |
| 40 | Preamp Gain |  | ok (2.0 dB) | ok (1.269%) |
| 41 | Preamp Bypass |  | ok (-6.2 dB) | ok (3.314%) |
| 42 | Preamp Circuit |  | ok (-8.6 dB) | ok (3.866%) |
| 43 | Preamp HQ | Preamp Circuit=1 | ok (-7.7 dB) | ok (3.622%) |
| 44 | Output Gain |  | ok (-0.6 dB) | ok (1.411%) |
| 45 | Auto Gain |  | ok (-18.3 dB) | ok (3.571%) |
| 46 | Delta |  | ok (-3.5 dB) | ok (2.861%) |
| 47 | Bypass |  | ok (-2.4 dB) | ok (3.222%) |
| 48 | Bypass |  | ok (-2.4 dB) | ok (3.179%) |
| 49 | Wet |  | ok (-2.4 dB) | ok (3.482%) |
| 50 | Delta |  | ok (-3.5 dB) | ok (1.419%) |
