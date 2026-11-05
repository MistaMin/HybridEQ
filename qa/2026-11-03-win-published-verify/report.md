# Plugin QA: VST3: HybridEQ

| Check | Status | Detail |
|---|---|---|
| loads | PASS | VST3: HybridEQ (OpenGrid), 49 parameters |
| finite output | PASS | peak -4.3 dBFS |
| not silent | PASS | rms -15.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1077x688, 0.1% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Oversample |  | ok (-14.2 dB) | ok (8.969%) |
| 1 | Oversample Filter | random state 2 | CONDITIONAL (-7.7 dB) | ok (4.646%) |
| 2 | 2nd Harmonic |  | ok (-5.3 dB) | ok (0.530%) |
| 3 | 3rd Harmonic |  | ok (-8.2 dB) | ok (6.681%) |
| 4 | 4th Harmonic |  | ok (-11.0 dB) | ok (4.174%) |
| 5 | 5th Harmonic |  | ok (-14.1 dB) | ok (7.709%) |
| 6 | Low Cut Freq |  | ok (-3.7 dB) | ok (11.015%) |
| 7 | Low Cut Q |  | ok (3.1 dB) | ok (4.314%) |
| 8 | Low Cut Slope |  | ok (-8.2 dB) | ok (8.140%) |
| 9 | Low Cut Mode |  | ok (-8.3 dB) | ok (3.896%) |
| 10 | Low Cut Bypass |  | ok (-7.6 dB) | ok (4.164%) |
| 11 | High Cut Freq |  | ok (-4.5 dB) | ok (4.878%) |
| 12 | High Cut Q |  | ok (3.9 dB) | ok (2.333%) |
| 13 | High Cut Slope |  | ok (-22.4 dB) | ok (2.129%) |
| 14 | High Cut Mode |  | ok (-17.0 dB) | ok (6.084%) |
| 15 | High Cut Bypass |  | ok (-14.9 dB) | ok (1.898%) |
| 16 | Low Freq | random state 1 | CONDITIONAL (6.0 dB) | ok (1.600%) |
| 17 | Low Gain |  | ok (0.7 dB) | ok (4.878%) |
| 18 | Low Type | random state 1 | CONDITIONAL (-0.6 dB) | ok (12.126%) |
| 19 | Low Mode | random state 1 | CONDITIONAL (5.6 dB) | ok (6.673%) |
| 20 | Low Band Bypass | random state 1 | CONDITIONAL (2.5 dB) | ok (3.076%) |
| 21 | Mid 1 Freq | random state 1 | CONDITIONAL (6.0 dB) | ok (6.680%) |
| 22 | Mid 1 Gain |  | ok (-0.0 dB) | ok (3.988%) |
| 23 | Mid 1 Q | random state 1 | CONDITIONAL (6.0 dB) | ok (1.695%) |
| 24 | Mid 1 Type | random state 1 | CONDITIONAL (6.0 dB) | ok (12.437%) |
| 25 | Mid 1 Mode | random state 1 | CONDITIONAL (6.0 dB) | ok (2.040%) |
| 26 | Mid 1 Bypass | random state 1 | CONDITIONAL (6.0 dB) | ok (4.640%) |
| 27 | Mid 2 Freq | random state 1 | CONDITIONAL (6.0 dB) | ok (7.932%) |
| 28 | Mid 2 Gain |  | ok (0.7 dB) | ok (4.854%) |
| 29 | Mid 2 Q | random state 1 | CONDITIONAL (4.8 dB) | ok (0.514%) |
| 30 | Mid 2 Type | random state 1 | CONDITIONAL (6.0 dB) | ok (14.608%) |
| 31 | Mid 2 Mode | random state 1 | CONDITIONAL (0.8 dB) | ok (3.274%) |
| 32 | Mid 2 Bypass | random state 1 | CONDITIONAL (5.7 dB) | ok (1.309%) |
| 33 | High Freq | random state 1 | CONDITIONAL (-20.2 dB) | ok (5.772%) |
| 34 | High Gain |  | ok (-0.8 dB) | ok (2.928%) |
| 35 | High Type | random state 1 | CONDITIONAL (-37.6 dB) | ok (13.748%) |
| 36 | High Mode | random state 1 | CONDITIONAL (-24.2 dB) | ok (1.942%) |
| 37 | High Band Bypass | random state 1 | CONDITIONAL (-25.6 dB) | ok (6.401%) |
| 38 | Preamp Type |  | ok (-5.5 dB) | ok (2.958%) |
| 39 | Preamp Pad |  | ok (-3.3 dB) | ok (2.035%) |
| 40 | Preamp Gain |  | ok (2.1 dB) | ok (5.280%) |
| 41 | Preamp Bypass |  | ok (-5.5 dB) | ok (3.247%) |
| 42 | Preamp Circuit |  | ok (-8.4 dB) | ok (8.827%) |
| 43 | Preamp HQ | random state 3 | CONDITIONAL (6.0 dB) | ok (5.030%) |
| 44 | Output Gain |  | ok (-0.6 dB) | ok (1.169%) |
| 45 | Bypass |  | ok (-2.2 dB) | ok (0.906%) |
| 46 | Bypass |  | ok (-2.2 dB) | ok (1.523%) |
| 47 | Wet |  | ok (-2.2 dB) | ok (2.061%) |
| 48 | Delta |  | ok (-3.2 dB) | ok (6.321%) |
