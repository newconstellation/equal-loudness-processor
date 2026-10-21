# Equal-Loudness Processor

Real-time system processor for live sound. It takes a stereo program feed and up to three measurement mics, and feeds mains L/R and a sub.

- **Loudness compensation** that follows the music. ISO 226 equal-loudness contours, driven by program level (auto-calibrated to SPL from the mics) with attack/release.
- **Room correction** from setup traces captured with pink noise on all three mics at once, or imported (Smaart, REW or any frequency/dB text). Optional **live trim** compares each mic against the processed program during the show, within a set range.
- 31-band minimum-phase graphic EQ whose band gains are solved so the cascade hits the curve at every band centre.
- Linkwitz-Riley 24 dB/oct crossover, mains/sub gain and delay, sub polarity, look-ahead peak limiter, pink noise generator.
- Zwicker loudness (ISO 532-1) readout.

Builds as a standalone Mac app, AU and VST3 (JUCE 8). GitHub Actions builds the Mac version on every push to `main`. See `INSTALL.txt`.
