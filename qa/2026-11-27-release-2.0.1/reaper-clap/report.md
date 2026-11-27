# Plugin QA: CLAP: HybridEQ

| Check | Status | Detail |
|---|---|---|
| loads | PASS | CLAP: HybridEQ (OpenGrid), 50 parameters |
| finite output | PASS | peak -4.1 dBFS |
| not silent | PASS | rms -15.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1062x845, 0.3% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Oversample |  | ok (-5.5 dB) | ok (5.023%) |
| 1 | Oversample Filter | Oversample=1 | ok (-2.3 dB) | ok (4.766%) |
| 2 | 2nd Harmonic |  | ok (-0.6 dB) | ok (2.652%) |
| 3 | 3rd Harmonic |  | ok (-8.8 dB) | ok (1.030%) |
| 4 | 4th Harmonic |  | ok (-10.8 dB) | ok (1.472%) |
| 5 | 5th Harmonic |  | ok (-13.6 dB) | ok (1.442%) |
| 6 | Low Cut Freq |  | ok (-3.7 dB) | ok (5.661%) |
| 7 | Low Cut Q |  | ok (3.1 dB) | ok (2.608%) |
| 8 | Low Cut Slope |  | ok (-8.3 dB) | ok (1.588%) |
| 9 | Low Cut Mode |  | ok (-8.3 dB) | ok (1.542%) |
| 10 | Low Cut Bypass |  | ok (-7.6 dB) | ok (1.944%) |
| 11 | High Cut Freq |  | ok (-4.6 dB) | ok (1.222%) |
| 12 | High Cut Q |  | ok (3.7 dB) | ok (1.425%) |
| 13 | High Cut Slope |  | ok (-21.8 dB) | ok (1.343%) |
| 14 | High Cut Mode |  | ok (-16.2 dB) | ok (1.370%) |
| 15 | High Cut Bypass |  | ok (-14.4 dB) | ok (3.988%) |
| 16 | Low Freq | Low Gain=1 | ok (1.0 dB) | ok (4.424%) |
| 17 | Low Gain |  | ok (0.6 dB) | ok (4.569%) |
| 18 | Low Type | Low Gain=1 | ok (-3.9 dB) | ok (11.589%) |
| 19 | Low Mode | Low Gain=1 | ok (0.9 dB) | ok (3.747%) |
| 20 | Low Band Bypass | random state 1 | CONDITIONAL (-7.4 dB) | ok (3.622%) |
| 21 | Mid 1 Freq | Mid 1 Gain=1 | ok (1.8 dB) | ok (3.737%) |
| 22 | Mid 1 Gain |  | ok (-0.0 dB) | ok (1.748%) |
| 23 | Mid 1 Q | Mid 1 Gain=1 | ok (0.9 dB) | ok (1.833%) |
| 24 | Mid 1 Type | Mid 1 Gain=1 | ok (-0.2 dB) | ok (9.425%) |
| 25 | Mid 1 Mode | Mid 1 Gain=1 | ok (2.3 dB) | ok (3.917%) |
| 26 | Mid 1 Bypass | random state 1 | CONDITIONAL (0.6 dB) | ok (4.287%) |
| 27 | Mid 2 Freq | Mid 2 Gain=1 | ok (2.6 dB) | ok (4.349%) |
| 28 | Mid 2 Gain |  | ok (0.8 dB) | ok (3.226%) |
| 29 | Mid 2 Q | Mid 2 Gain=1 | ok (0.6 dB) | ok (4.248%) |
| 30 | Mid 2 Type | Mid 2 Gain=1 | ok (0.6 dB) | ok (10.933%) |
| 31 | Mid 2 Mode | Mid 2 Gain=1 | ok (2.1 dB) | ok (4.076%) |
| 32 | Mid 2 Bypass | random state 1 | CONDITIONAL (-4.6 dB) | ok (4.748%) |
| 33 | High Freq | High Gain=1 | ok (1.9 dB) | ok (4.019%) |
| 34 | High Gain |  | ok (-1.3 dB) | ok (2.273%) |
| 35 | High Type | High Gain=1 | ok (-5.8 dB) | ok (10.579%) |
| 36 | High Mode | High Gain=1 | ok (-2.1 dB) | ok (2.529%) |
| 37 | High Band Bypass | random state 1 | CONDITIONAL (-29.6 dB) | ok (1.733%) |
| 38 | Preamp Type |  | ok (-6.2 dB) | ok (5.813%) |
| 39 | Preamp Pad |  | ok (-3.4 dB) | ok (4.229%) |
| 40 | Preamp Gain |  | ok (2.0 dB) | ok (5.087%) |
| 41 | Preamp Bypass |  | ok (-6.2 dB) | ok (3.351%) |
| 42 | Preamp Circuit |  | ok (-2.4 dB) | ok (4.976%) |
| 43 | Preamp HQ | Preamp Circuit=1 | ok (-2.8 dB) | ok (5.100%) |
| 44 | Output Gain |  | ok (-0.6 dB) | ok (2.712%) |
| 45 | Auto Gain |  | ok (-18.3 dB) | ok (1.613%) |
| 46 | Delta |  | ok (-3.5 dB) | ok (3.900%) |
| 47 | Bypass |  | ok (-2.4 dB) | ok (2.187%) |
| 48 | Wet |  | ok (-2.4 dB) | ok (3.579%) |
| 49 | Delta |  | ok (-3.5 dB) | ok (3.812%) |
