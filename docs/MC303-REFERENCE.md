# MC-303 sound-family study

Reference: Roland's [MC-303 Owner's Manual](https://static.roland.com/assets/media/pdf/MC-303_OM.pdf), printed pages 85–89, inspected on 2026-10-14. The scanned tone list describes dedicated lead/bass/stack/pad groups, then strings, vocal/FX/noise, keys, organ, tuned percussion, guitar, brass, ethnic instruments, hits and drum/percussion groups.

MC-1 covers useful synthesis families with 48 independently designed presets; it does not reproduce the complete original bank, parameter data, samples or exact sounds. It uses no FM engine. Names and parameters in presets.json are original.

| Manual group | MC-1 coverage | Boundary |
|---|---|---|
| 01–02, synthesizer and synth bass | Saw/pulse leads, resonant mono bass, deep bass, short sequences | No circuit or factory-tone matching |
| 03–04, bass guitar and stacks | Plucked bass and polyphonic synth textures | Synthesized approximations; no multisamples or multi-tone patch layers |
| 05–06, pads and strings | Slow, warm, bell and string-like pads | No sampled orchestral sections or per-voice LFO |
| 07–09, voice/FX/noise | Abstract digital/air textures only | No recognizable vocal syllables, speech, scratch or dedicated noise oscillator |
| 10–13, keys/organ/mallet/bell | Hammer/tine/clav-like keys, organ-like sustains, wooden and glass attacks | Stylized synthesis, not acoustic piano realism or drawbar emulation |
| 14–17, guitar/brass/ethnic | Plucked string, brass-like, reed/pipe and thumb-pluck families | No sampled guitar articulations, saxophone or ensemble realism |
| 18–29, hits/SFX/percussion | One synth stab family and 16 synthesized drum lanes | No human/scratch/nature SFX or 12 recreated rhythm kits |

The 36 LA-inspired presets share twelve original mathematical PCM attacks (512 samples each, signed 16-bit; 12,288 flash bytes). The twelve 303-inspired presets use the existing mono subtractive engine. First twelve preset indexes and first six PCM attacks remain unchanged for the existing factory demos. The additional bank adds constant patch/PCM data, with no larger voice pool, runtime FX buffers or project struct. Browser and hardware use the same bank.

This expansion is not a claim that 48 is a flash limit. More ROM data fits within the app slot; more samples alone do not add missing modulation, layers or realistic instrument behavior. Device CPU load, audio deadlines and musical listening quality must still be measured on FM-1.
