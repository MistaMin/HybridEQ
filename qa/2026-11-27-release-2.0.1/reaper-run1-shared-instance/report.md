# Plugin QA: CLAP: HybridEQ

| Check | Status | Detail |
|---|---|---|
| loads | PASS | CLAP: HybridEQ (OpenGrid), 50 parameters |
| finite output | PASS | peak -4.1 dBFS |
| not silent | PASS | rms -15.3 dBFS |
| deterministic render | FAIL | repeat differs by -8.0 dB |
| editor opens | PASS | 1062x845, 0.0% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | FAIL | Oversample, Oversample Filter, 2nd Harmonic, 3rd Harmonic, 4th Harmonic, 5th Harmonic, Low Cut Freq, Low Cut Q, Low Cut Slope, Low Cut Mode, Low Cut Bypass, High Cut Freq, High Cut Q, High Cut Slope, High Cut Mode, High Cut Bypass, Low Freq, Low Gain, Low Type, Low Mode, Low Band Bypass, Mid 1 Freq, Mid 1 Gain, Mid 1 Q, Mid 1 Type, Mid 1 Mode, Mid 1 Bypass, Mid 2 Freq, Mid 2 Gain, Mid 2 Q, Mid 2 Type, Mid 2 Mode, Mid 2 Bypass, High Freq, High Gain, High Type, High Mode, High Band Bypass, Preamp Type, Preamp Pad, Preamp Gain, Preamp Bypass, Preamp Circuit, Preamp HQ, Output Gain, Auto Gain, Delta, Bypass, Wet, Delta |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Oversample |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 1 | Oversample Filter | Oversample=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 2 | 2nd Harmonic |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 3 | 3rd Harmonic |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 4 | 4th Harmonic |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 5 | 5th Harmonic |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 6 | Low Cut Freq |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 7 | Low Cut Q |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 8 | Low Cut Slope |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 9 | Low Cut Mode |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 10 | Low Cut Bypass |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 11 | High Cut Freq |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 12 | High Cut Q |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 13 | High Cut Slope |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 14 | High Cut Mode |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 15 | High Cut Bypass |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 16 | Low Freq | Low Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 17 | Low Gain |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 18 | Low Type | Low Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 19 | Low Mode | Low Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 20 | Low Band Bypass |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 21 | Mid 1 Freq | Mid 1 Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 22 | Mid 1 Gain |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 23 | Mid 1 Q | Mid 1 Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 24 | Mid 1 Type | Mid 1 Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 25 | Mid 1 Mode | Mid 1 Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 26 | Mid 1 Bypass |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 27 | Mid 2 Freq | Mid 2 Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 28 | Mid 2 Gain |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 29 | Mid 2 Q | Mid 2 Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 30 | Mid 2 Type | Mid 2 Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 31 | Mid 2 Mode | Mid 2 Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 32 | Mid 2 Bypass |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 33 | High Freq | High Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 34 | High Gain |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 35 | High Type | High Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 36 | High Mode | High Gain=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 37 | High Band Bypass |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 38 | Preamp Type |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 39 | Preamp Pad |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 40 | Preamp Gain |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 41 | Preamp Bypass |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 42 | Preamp Circuit |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 43 | Preamp HQ | Preamp Circuit=1 | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 44 | Output Gain |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 45 | Auto Gain |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 46 | Delta |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 47 | Bypass |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 48 | Wet |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
| 49 | Delta |  | skipped (render not deterministic)  | NOT SHOWN (0.000%) |
