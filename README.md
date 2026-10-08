[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/YiPaBLlm)
# Real-Time Audio Synthesizer
## Team JoJo - PSoC 6 Project

A real-time audio synthesizer implemented on a PSoC 6 MCU. The system generates multiple waveforms and allows real-time control of pitch and effects using CapSense inputs, with visual feedback via a TFT display.

---

## 🎥 Demo Video

A full demonstration of the working synthesizer is available here:

👉 [![Watch the Synthesizer Demo](https://img.youtube.com/vi/Pe7vjXZtlxs/0.jpg)](https://youtube.com/shorts/Pe7vjXZtlxs)
---

## 📋 Project Overview

This project implements a real-time audio synthesizer with the following features:

- **Waveform Generation**: Sine, square, and sawtooth waveforms
- **Real-Time Pitch Control**: Frequency selection using CapSense slider with predefined pitch table
- **Waveform Selection**: Cycle through waveforms using CapSense button 0
- **Audio Effect**: Tremolo effect controlled via CapSense button 1
- **User Interface**: TFT display showing waveform, frequency, and system state
- **Audio Control**: Standard button for start/stop audio output

---

## 🎛️ Controls

| Control | Function |
|---------|----------|
| **User Button** | Toggle audio playback |
| **CapSense Slider** | Select pitch (mapped to 16-step pitch table) |
| **CapSense Button 1** | Switch waveform (Sine → Square → Saw) |
| **CapSense Button 2** | Toggle tremolo effect |
| **TFT Display** | Shows waveform, frequency, and status |

---

## ⚙️ Implementation Details

Waveforms are generated in real time using a 64-sample lookup table combined with lightweight integer arithmetic. This ensures stable timing and avoids heavy math operations during audio generation.

Pitch selection is implemented using a predefined frequency table, allowing fast switching without runtime computation overhead.

The system uses FreeRTOS to separate:
- audio generation
- input processing and UI updates

This ensures stable real-time performance.

---

## ⚡ Real-Time Design

- Fixed buffer audio processing (1024 samples)
- I2S streaming output
- CapSense interrupt-driven input updates
- No floating-point math in audio path
- Optimized for deterministic timing

---

## 📊 Summary

This project demonstrates a real-time embedded synthesizer with multiple waveforms, CapSense control, and TFT visualization. The design prioritizes simplicity, stability, and real-time audio performance.
