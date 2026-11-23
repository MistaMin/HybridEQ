# Plugin QA: AU: HybridEQ

| Check | Status | Detail |
|---|---|---|
| loads | PASS | AU: HybridEQ (OpenGrid), 50 parameters |
| finite output | PASS | peak -4.1 dBFS |
| not silent | PASS | rms -15.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1062x845, 6.9% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | FAIL | Oversample Filter (off -> on), Low Cut Mode (Stereo -> Mid), Low Cut Bypass (off -> on), High Cut Slope (-18 dB/oct -> -24 dB/oct), High Cut Mode (Stereo -> Mid), High Cut Bypass (off -> on), Low Type (Brit -> FSF), Low Band Bypass (off -> on), Mid 1 Mode (Stereo -> Mid), Mid 1 Bypass (off -> on), Mid 2 Bypass (off -> on), High Mode (Stereo -> Mid), High Band Bypass (off -> on), Preamp Type (Off -> Brit), Preamp Circuit (off -> on), Auto Gain (off -> on) |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Oversample |  | ok (-7.4 dB) | ok (12.158%) |
| 1 | Oversample Filter | Oversample=1 | ok (-0.3 dB) | ok (12.035%) |
| 2 | 2nd Harmonic |  | ok (-5.6 dB) | ok (12.115%) |
| 3 | 3rd Harmonic |  | ok (-8.8 dB) | ok (11.904%) |
| 4 | 4th Harmonic |  | ok (-10.8 dB) | ok (11.764%) |
| 5 | 5th Harmonic |  | ok (-13.6 dB) | ok (11.710%) |
| 6 | Low Cut Freq |  | ok (-3.7 dB) | ok (12.793%) |
| 7 | Low Cut Q |  | ok (3.1 dB) | ok (12.672%) |
| 8 | Low Cut Slope |  | ok (-8.3 dB) | ok (11.981%) |
| 9 | Low Cut Mode |  | ok (-8.3 dB) | ok (11.922%) |
| 10 | Low Cut Bypass |  | ok (-7.6 dB) | ok (11.516%) |
| 11 | High Cut Freq |  | ok (-4.6 dB) | ok (12.132%) |
| 12 | High Cut Q |  | ok (3.7 dB) | ok (12.039%) |
| 13 | High Cut Slope |  | ok (-21.8 dB) | ok (11.956%) |
| 14 | High Cut Mode |  | ok (-16.2 dB) | ok (11.932%) |
| 15 | High Cut Bypass |  | ok (-14.4 dB) | ok (11.293%) |
| 16 | Low Freq | Low Gain=1 | ok (1.0 dB) | ok (12.193%) |
| 17 | Low Gain |  | ok (0.6 dB) | ok (12.771%) |
| 18 | Low Type | Low Gain=1 | ok (-3.9 dB) | ok (7.435%) |
| 19 | Low Mode | Low Gain=1 | ok (0.9 dB) | ok (12.914%) |
| 20 | Low Band Bypass | random state 1 | CONDITIONAL (-7.4 dB) | ok (11.447%) |
| 21 | Mid 1 Freq | Mid 1 Gain=1 | ok (1.8 dB) | ok (12.080%) |
| 22 | Mid 1 Gain |  | ok (-0.0 dB) | ok (11.943%) |
| 23 | Mid 1 Q | Mid 1 Gain=1 | ok (0.9 dB) | ok (11.966%) |
| 24 | Mid 1 Type | Mid 1 Gain=1 | ok (-0.2 dB) | ok (17.743%) |
| 25 | Mid 1 Mode | Mid 1 Gain=1 | ok (2.3 dB) | ok (11.999%) |
| 26 | Mid 1 Bypass | random state 1 | CONDITIONAL (0.6 dB) | ok (11.005%) |
| 27 | Mid 2 Freq | Mid 2 Gain=1 | ok (2.6 dB) | ok (12.217%) |
| 28 | Mid 2 Gain |  | ok (0.8 dB) | ok (12.059%) |
| 29 | Mid 2 Q | Mid 2 Gain=1 | ok (0.6 dB) | ok (11.967%) |
| 30 | Mid 2 Type | Mid 2 Gain=1 | ok (0.6 dB) | ok (17.409%) |
| 31 | Mid 2 Mode | Mid 2 Gain=1 | ok (2.1 dB) | ok (11.972%) |
| 32 | Mid 2 Bypass | random state 1 | CONDITIONAL (-4.6 dB) | ok (10.929%) |
| 33 | High Freq | High Gain=1 | ok (1.9 dB) | ok (11.908%) |
| 34 | High Gain |  | ok (-1.3 dB) | ok (12.049%) |
| 35 | High Type | High Gain=1 | ok (-5.8 dB) | ok (17.975%) |
| 36 | High Mode | High Gain=1 | ok (-2.1 dB) | ok (12.211%) |
| 37 | High Band Bypass | random state 1 | CONDITIONAL (-29.6 dB) | ok (11.373%) |
| 38 | Preamp Type |  | ok (-6.2 dB) | ok (14.259%) |
| 39 | Preamp Pad |  | ok (-3.4 dB) | ok (11.816%) |
| 40 | Preamp Gain |  | ok (2.0 dB) | ok (11.769%) |
| 41 | Preamp Bypass |  | ok (-6.2 dB) | ok (12.255%) |
| 42 | Preamp Circuit |  | ok (-8.6 dB) | ok (11.892%) |
| 43 | Preamp HQ | Preamp Circuit=1 | ok (-2.8 dB) | ok (12.357%) |
| 44 | Output Gain |  | ok (-0.6 dB) | ok (12.120%) |
| 45 | Auto Gain |  | ok (-18.3 dB) | ok (11.838%) |
| 46 | Delta |  | ok (-3.5 dB) | ok (12.148%) |
| 47 | Bypass |  | ok (-2.4 dB) | ok (11.864%) |
| 48 | Wet |  | ok (-2.4 dB) | ok (12.148%) |
| 49 | Delta |  | ok (-2.4 dB) | ok (11.899%) |
