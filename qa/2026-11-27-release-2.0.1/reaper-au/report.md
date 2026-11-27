# Plugin QA: AU: HybridEQ

| Check | Status | Detail |
|---|---|---|
| loads | PASS | AU: HybridEQ (OpenGrid), 50 parameters |
| finite output | PASS | peak -4.1 dBFS |
| not silent | PASS | rms -15.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1062x845, 0.5% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Oversample |  | ok (-2.8 dB) | ok (1.421%) |
| 1 | Oversample Filter | Oversample=1 | ok (-0.3 dB) | ok (3.548%) |
| 2 | 2nd Harmonic |  | ok (-0.6 dB) | ok (0.923%) |
| 3 | 3rd Harmonic |  | ok (-8.8 dB) | ok (2.937%) |
| 4 | 4th Harmonic |  | ok (-10.8 dB) | ok (1.743%) |
| 5 | 5th Harmonic |  | ok (-13.6 dB) | ok (3.254%) |
| 6 | Low Cut Freq |  | ok (-3.7 dB) | ok (4.008%) |
| 7 | Low Cut Q |  | ok (3.1 dB) | ok (4.269%) |
| 8 | Low Cut Slope |  | ok (-8.3 dB) | ok (2.789%) |
| 9 | Low Cut Mode |  | ok (-8.3 dB) | ok (0.418%) |
| 10 | Low Cut Bypass |  | ok (-7.6 dB) | ok (3.618%) |
| 11 | High Cut Freq |  | ok (-4.6 dB) | ok (1.583%) |
| 12 | High Cut Q |  | ok (3.7 dB) | ok (1.115%) |
| 13 | High Cut Slope |  | ok (-21.8 dB) | ok (3.244%) |
| 14 | High Cut Mode |  | ok (-16.2 dB) | ok (1.861%) |
| 15 | High Cut Bypass |  | ok (-14.4 dB) | ok (1.103%) |
| 16 | Low Freq | Low Gain=1 | ok (1.0 dB) | ok (3.176%) |
| 17 | Low Gain |  | ok (0.6 dB) | ok (2.526%) |
| 18 | Low Type | Low Gain=1 | ok (-3.9 dB) | ok (10.518%) |
| 19 | Low Mode | Low Gain=1 | ok (0.9 dB) | ok (2.391%) |
| 20 | Low Band Bypass | random state 1 | CONDITIONAL (-7.4 dB) | ok (1.362%) |
| 21 | Mid 1 Freq | Mid 1 Gain=1 | ok (1.8 dB) | ok (3.088%) |
| 22 | Mid 1 Gain |  | ok (-0.0 dB) | ok (1.982%) |
| 23 | Mid 1 Q | Mid 1 Gain=1 | ok (0.9 dB) | ok (1.003%) |
| 24 | Mid 1 Type | Mid 1 Gain=1 | ok (-0.2 dB) | ok (2.065%) |
| 25 | Mid 1 Mode | Mid 1 Gain=1 | ok (2.3 dB) | ok (1.711%) |
| 26 | Mid 1 Bypass | random state 1 | CONDITIONAL (0.6 dB) | ok (4.260%) |
| 27 | Mid 2 Freq | Mid 2 Gain=1 | ok (2.6 dB) | ok (1.393%) |
| 28 | Mid 2 Gain |  | ok (0.8 dB) | ok (4.014%) |
| 29 | Mid 2 Q | Mid 2 Gain=1 | ok (0.6 dB) | ok (4.113%) |
| 30 | Mid 2 Type | Mid 2 Gain=1 | ok (0.6 dB) | ok (10.482%) |
| 31 | Mid 2 Mode | Mid 2 Gain=1 | ok (2.1 dB) | ok (2.183%) |
| 32 | Mid 2 Bypass | random state 1 | CONDITIONAL (-4.6 dB) | ok (3.499%) |
| 33 | High Freq | High Gain=1 | ok (1.9 dB) | ok (4.098%) |
| 34 | High Gain |  | ok (-1.3 dB) | ok (2.244%) |
| 35 | High Type | High Gain=1 | ok (-5.8 dB) | ok (9.220%) |
| 36 | High Mode | High Gain=1 | ok (-2.1 dB) | ok (3.604%) |
| 37 | High Band Bypass | random state 1 | CONDITIONAL (-29.6 dB) | ok (1.954%) |
| 38 | Preamp Type |  | ok (-6.2 dB) | ok (3.488%) |
| 39 | Preamp Pad |  | ok (-3.4 dB) | ok (1.086%) |
| 40 | Preamp Gain |  | ok (2.0 dB) | ok (2.322%) |
| 41 | Preamp Bypass |  | ok (-6.2 dB) | ok (1.471%) |
| 42 | Preamp Circuit |  | ok (-8.6 dB) | ok (3.713%) |
| 43 | Preamp HQ | Preamp Circuit=1 | ok (-2.8 dB) | ok (4.053%) |
| 44 | Output Gain |  | ok (-0.6 dB) | ok (0.893%) |
| 45 | Auto Gain |  | ok (-18.3 dB) | ok (3.312%) |
| 46 | Delta |  | ok (-3.5 dB) | ok (2.637%) |
| 47 | Bypass |  | ok (-2.4 dB) | ok (0.908%) |
| 48 | Wet |  | ok (-2.4 dB) | ok (3.684%) |
| 49 | Delta |  | ok (-3.5 dB) | ok (1.638%) |
