# Plugin QA: VST3: HybridEQ

| Check | Status | Detail |
|---|---|---|
| loads | PASS | VST3: HybridEQ (OpenGrid), 51 parameters |
| finite output | PASS | peak -4.1 dBFS |
| not silent | PASS | rms -15.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1062x845, 0.1% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | FAIL | Oversample (1x -> 4x), 2nd Harmonic (87 -> 80), 3rd Harmonic (24 -> 94), 4th Harmonic (61 -> 74), 5th Harmonic (98 -> 92), Low Cut Freq (78.6 -> 22.4), Low Cut Q (5.23 -> 2.25), Low Cut Slope (-6 dB/oct -> -24 dB/oct), Low Cut Bypass (On -> Off), High Cut Freq (1.97 k -> 21.4 k), High Cut Q (3.32 -> 0.23), High Cut Slope (-24 dB/oct -> -12 dB/oct), High Cut Mode (Mid -> Stereo), High Cut Bypass (On -> Off), Low Freq (23.2 -> 102), Low Gain (-2.9 -> +2.7), Low Type (FSF -> Bax-EQ), Low Band Bypass (On -> Off), Mid 1 Freq (4.27 k -> 517), Mid 1 Gain (-8.3 -> +15.0) |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Oversample |  | ok (-7.4 dB) | ok (3.542%) |
| 1 | Oversample Filter | Oversample=1 | ok (-0.3 dB) | ok (3.972%) |
| 2 | 2nd Harmonic |  | ok (-5.6 dB) | ok (2.101%) |
| 3 | 3rd Harmonic |  | ok (-8.8 dB) | ok (1.638%) |
| 4 | 4th Harmonic |  | ok (-10.8 dB) | ok (1.341%) |
| 5 | 5th Harmonic |  | ok (-13.6 dB) | ok (1.598%) |
| 6 | Low Cut Freq |  | ok (-3.7 dB) | ok (4.676%) |
| 7 | Low Cut Q |  | ok (3.1 dB) | ok (2.293%) |
| 8 | Low Cut Slope |  | ok (-8.3 dB) | ok (1.505%) |
| 9 | Low Cut Mode |  | ok (-8.3 dB) | ok (1.040%) |
| 10 | Low Cut Bypass |  | ok (-7.6 dB) | ok (1.800%) |
| 11 | High Cut Freq |  | ok (-4.6 dB) | ok (1.644%) |
| 12 | High Cut Q |  | ok (3.7 dB) | ok (1.655%) |
| 13 | High Cut Slope |  | ok (-21.8 dB) | ok (1.222%) |
| 14 | High Cut Mode |  | ok (-16.2 dB) | ok (0.931%) |
| 15 | High Cut Bypass |  | ok (-14.4 dB) | ok (1.968%) |
| 16 | Low Freq | Low Gain=1 | ok (1.0 dB) | ok (1.735%) |
| 17 | Low Gain |  | ok (0.6 dB) | ok (2.763%) |
| 18 | Low Type | Low Gain=1 | ok (-3.9 dB) | ok (8.844%) |
| 19 | Low Mode | Low Gain=1 | ok (0.9 dB) | ok (2.742%) |
| 20 | Low Band Bypass | random state 1 | CONDITIONAL (-7.4 dB) | ok (1.472%) |
| 21 | Mid 1 Freq | Mid 1 Gain=1 | ok (1.8 dB) | ok (1.630%) |
| 22 | Mid 1 Gain |  | ok (-0.0 dB) | ok (1.850%) |
| 23 | Mid 1 Q | Mid 1 Gain=1 | ok (0.9 dB) | ok (1.540%) |
| 24 | Mid 1 Type | Mid 1 Gain=1 | ok (-0.2 dB) | ok (10.541%) |
| 25 | Mid 1 Mode | Mid 1 Gain=1 | ok (2.3 dB) | ok (1.961%) |
| 26 | Mid 1 Bypass | random state 1 | CONDITIONAL (0.6 dB) | ok (2.373%) |
| 27 | Mid 2 Freq | Mid 2 Gain=1 | ok (2.6 dB) | ok (5.124%) |
| 28 | Mid 2 Gain |  | ok (0.8 dB) | ok (2.999%) |
| 29 | Mid 2 Q | Mid 2 Gain=1 | ok (0.6 dB) | ok (2.124%) |
| 30 | Mid 2 Type | Mid 2 Gain=1 | ok (0.6 dB) | ok (8.068%) |
| 31 | Mid 2 Mode | Mid 2 Gain=1 | ok (2.1 dB) | ok (1.595%) |
| 32 | Mid 2 Bypass | random state 1 | CONDITIONAL (-4.6 dB) | ok (2.170%) |
| 33 | High Freq | High Gain=1 | ok (1.9 dB) | ok (2.309%) |
| 34 | High Gain |  | ok (-1.3 dB) | ok (1.891%) |
| 35 | High Type | High Gain=1 | ok (-5.8 dB) | ok (7.815%) |
| 36 | High Mode | High Gain=1 | ok (-2.1 dB) | ok (1.900%) |
| 37 | High Band Bypass | random state 1 | CONDITIONAL (-29.6 dB) | ok (1.559%) |
| 38 | Preamp Type |  | ok (-6.2 dB) | ok (3.417%) |
| 39 | Preamp Pad |  | ok (-3.4 dB) | ok (1.533%) |
| 40 | Preamp Gain |  | ok (2.0 dB) | ok (1.116%) |
| 41 | Preamp Bypass |  | ok (-6.2 dB) | ok (3.587%) |
| 42 | Preamp Circuit |  | ok (-8.6 dB) | ok (4.134%) |
| 43 | Preamp HQ | Preamp Circuit=1 | ok (-2.8 dB) | ok (3.765%) |
| 44 | Output Gain |  | ok (-0.6 dB) | ok (2.280%) |
| 45 | Auto Gain |  | ok (-18.3 dB) | ok (1.623%) |
| 46 | Delta |  | ok (-3.5 dB) | ok (1.755%) |
| 47 | Bypass |  | ok (-2.4 dB) | ok (1.467%) |
| 48 | Bypass |  | ok (-2.4 dB) | ok (1.370%) |
| 49 | Wet |  | ok (-2.4 dB) | ok (1.401%) |
| 50 | Delta |  | ok (-3.5 dB) | ok (1.401%) |
