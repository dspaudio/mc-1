# MC-1 validation record

## 성능 최적화·USB OTA 통합 — 2026-10-14

기존 로컬 수정 위에서 LA 기록 PCM의 mix 127 경로가 버리는 오실레이터 평가와
코러스/플랜저 amount 0의 wet 보간만 생략했다. 위상·샘플 위치·루프·필터·
이펙트 이력과 정수 라운딩은 보존한다. 정상 LCD 전송은 단일 버퍼의 DMA 완료를
확인해 루프당 8행씩 진행하고, 마지막 완료 관측 후 50ms 간격을 둔다.

앱은 `FELUCCA_OTA=1`이다. JEDEC 실패는 오디오 정지 전에 요청을 소비하고,
정상 업데이트는 재생을 정지한다. 실패하면 오디오·UART·일반 USB를 재개하되
재생은 정지 상태다. 큐에 게시된 MIDI 정리는 전송 완료로 취급하지 않고,
폐기 후 Stop·CC64/120/123을 새 Note On보다 먼저 다시 게시한다.
프로젝트 복원·IRQ 초기화 뒤 부팅 staging 정리도 실행한다.

기록 섹터의 삭제 및 4KiB 전체 readback 확인이 로더 변경의 선행 조건이다.
지속적인 삭제·읽기·불일치 34개 위치와 재시도에서 로더 보존을 검사했다.
단발 기록 검증 오류와 정리까지 지속되는 오류를 별도로 검사한다. 무효
패키지 거절은 기존 복구 상태를 지우지 않는다. 로더 본문은 16,352바이트
이하이며 헤더와 함께 `0xE4000` 전에 끝난다. 더 큰 공식 stock loader의
원본 V15 패키지를 수용하는 경로로 주장하지 않는다.

최종 소스의 호스트 16개, 새 Homebrew LLVM ASan/UBSan 16개, 새 패키지를
사용한 sanitized OTA·loader 2개, HTTP 및 오디오 렌더 129개가 통과했다.
웹 공유 파일 18개가 일치하며 새 WASM과 AudioWorklet·MIDI·패널 등 9개
회귀도 통과했다. 오디오 측정 보고서는 수정 전과 동일하다.
독립 128프리셋/mix 전환/release PCM SHA-256은
`1c0b50337a12e354bf8e4a1778bab1bc165030033b3e039fe7f8058dad272971`로 동일하다.

동일 호스트의 8보이스 PCM 10초 렌더 7회 CPU 중앙값은
0.059632초에서 0.048633초로 줄었다. 128프리셋·idle·전체 FX mix의
명령어 비교 130개에서 +25% 초과는 없다. 이는 FM-1 CPU 기한의 증명이 아니다.

Orbit의 역방향 분기 구간 휴리스틱을 그대로 적용하면 `mc_render` 비용은
3980→6280으로 +10% 기준을 초과한다. 이를 통과했다고 표시하지 않는다.
실제 점프 테이블과 지배 관계를 분석하면 새 역방향 재합류 3개는 반복이 아니다.
동일 DIV=9/NEST=4로 반복 구간을 계산하면 3803→3643이며, 새 `recorded`
callee의 47개 명령과 나눗셈 1개의 추가 가중치를 두 호출 지점에 더한
구간 추정은 4083(+7.36%)다. 전체 자연 루프 노드를 포함하는 별도 모델은
3944→4236(+7.40%)다. 이 값들은 기존 callee 전체·루프 밖 작업·메모리 대기를
포괄하는 렌더 시간 상한이 아니다. 최초 계산의 나눗셈 누락은 독립 검토로 정정했다.
ISR 비용은 193→193, 이펙트는 782→796이다. 정적 추정은 실제 사이클이나
XIP 대기·IRQ 지연을 측정하지 않으며, 실기기 오디오 기한은 미검증이다.

pi32v2 앱 490,740바이트, globals 50,036바이트, pool 304,708바이트,
RAM 코드 5,568바이트 및 8,212바이트 RAM loader의 메모리·flash-off call
가드와 SDK 3개 해시·LZ4 roundtrip이 통과했다. 패키지는 610,089바이트이며
SHA-256은 `919ca955192d56f14e9e200c48b161b08e4aa8f20e8b27fbaafdc113ce31ec6c`다.
측정 JSON은 갱신했다. compiler 경로는 기존 CI 표기를 유지한다.

원본 Felucca `a53ecf428c29c780108fd70a31b71a32a6b5c313`와 로컬 Orbit을
대조했다. 공식 포럼 가이드는 참고만 했고, 공식 저장소는 조회 시 404여서
코드를 직접 검토한 근거로 사용하지 않았다.
이번 작업에서는 실기기 쓰기·커밋·푸시·배포를 하지 않았다. 실제 재업데이트·
부팅·전원 차단 복구·장시간 오디오·IRQ/USB 재열거·스택 최대 사용량은 미검증이다.

## 0.3.0 설치·MIDI 안전성 수정 — 2026-10-14

설치기에서 JEDEC 식별 실패 시 복구 기록을 유지하고, 호스트의 `success\0`
응답과 복구 기록 삭제·읽기 검증이 끝나야 재부팅하도록 수정했다. 시작 시 RAM
복구 기록도 보존한다. 여러 기록을 정리할 때는 섹터마다 USB와 watchdog을 서비스한다.
호스트 오류 주입은 부분 설치 후 재시도, 잘못된 완료 응답, 35개 정리 실패 위치,
여러 기록의 부분 정리와 재시도, 17개 섹터에 각각 500 ms 삭제 지연을 포함한다.
보호 영역의 보존도 확인했다. watchdog 서비스 수정을 제거하면 지연 회귀가 실패한다.

저장과 데모 교체는 UART RX/DMA를 멈추고 입력 음·sustain·ARP·파서를 정리한다.
재개 전에는 소프트웨어 큐와 보류된 USB 입력을 폐기하고 UART DMA 커서를 초기화한다.
실제 HAL 코드의 모의 레지스터 테스트에서 1,572바이트(약 503 ms) 수신 시도를
중단 구간에 주입하고, 재개 후 새 Note On/Off가 정상 처리됨을 확인했다.
라우팅 테스트는 실제 C 엔진과 연결하여 입력 정리와 렌더 후 무음을 검사한다.
부팅 프로젝트 복원은 오디오/MIDI 시작 전에 완료하며 읽기 중 IRQ를 켜지 않는다.

데모 교체는 소비자에게 이미 넘긴 Note On의 채널만 추적하여 이전 음을 정리한다.
아직 전달되지 않은 정리도 재로드를 거쳐 유지하고 새 연주보다 먼저 출력한다.
held key, latched ARP, sustain, 출력 정체, 재로드, 큐 overflow를 검사했다.
웹은 동일 코어를 사용하며 실제 AudioWorklet에서 두 데모 × 세 채널 × ACK 정상/지연의
12개 회귀가 통과했다. 이전 WASM에서는 이전 음이 남는 실패를 먼저 재현했다.

새로 빌드한 호스트 12개 테스트, Homebrew LLVM ASan/UBSan 12개 테스트,
HTTP 통합, pi32v2 메모리 가드와 패키지 생성, 새 패키지를 사용한 sanitized 설치기
시뮬레이션이 통과했다. 웹의 네이티브 검사 2개와 Node 검사 9개도 통과했다.
소스와 웹 코어 15개 파일의 바이트 일치를 확인했다. 독립 통합 검토에서도
추가 확정 결함은 발견하지 못했다.

129개 오디오 렌더와 7개 청취 예제도 다시 생성했다. 오디오 측정 보고서와
청취 예제 파일은 수정 전과 바이트 단위로 동일하다.

현재 앱은 482,684바이트(슬롯 여유 98,880바이트), globals 46,656바이트,
pool 304,708바이트, RAM 코드 2,784바이트이며 정적 합계는 354,148바이트다.
로더는 8,212바이트, BSS 끝은 `0x01C0F3F0`이다. staged loader 끝(미포함)은
`0xE1B15`로 복구 기록 섹터 `0xE4000`과 겹치지 않는다.
현재 해시와 크기는 `target-report.json`, `package-report.json`에 갱신했다.
타깃 보고서의 compiler 경로는 기존 CI 표기를 유지한다.

실기기 플래시는 하지 않았다. 실제 UART DMA 재시작, USB 열거·저장 중 연결 유지,
전원 차단·watchdog 복구, 오디오 처리 기한 및 스택 최대 사용량은 아직 미검증이다.

## 0.3.0

Song mode stores 128 pattern/repeat entries in a 35,114-byte version 6 record.
Tests exercise empty songs, every entry, pattern 64, 1–16 repeats, bar-boundary
advance, end stop, restart/continue, looping, edits, malformed records and v5
imports. Song editing/save also runs through the actual native HTTP service.
The two existing 36 KiB flash banks are unchanged; storage fault injection covers
463 erase/program/header interruption points with the expanded record.

MIDI tests exercise separate source parsers, running status and interleaved
realtime bytes, notes/velocity, program and controller changes, sustain, pitch
bend, recording, generated ARP output, note-off ownership, queue overflow recovery
and transport at 60/120/240 BPM. Web AudioWorklet tests compare PCM, MIDI packets
and saved projects with native C, including disconnect and bounded queues.

Actual USB driver tests inspect composite MIDI/UAC descriptors, 44.1 kHz PCM
packetization, saturation, stream pause/re-prime and FIFO boundaries. UART tests
exercise running status and realtime parsing; routing tests verify USB/TRS source
identity, reset handling and output backpressure. These are host tests of the
driver code, not USB enumeration or electrical TRS tests.

Native LCD renders cover empty/editing/playing/completed songs, position 128,
16 repeats, MIDI clock/channel controls and melodic ADSR. Browser automation was
not used. Existing 129 audio-render reports are byte-identical to 0.2.0.

Onboard BLE MIDI is not implemented: the current runtime does not initialize
the official SDK Bluetooth controller, stack, RTOS or RF. See [MIDI.md](MIDI.md)
for source evidence and the distinction from OS-provided Web MIDI ports.
No device was flashed. FM-1 USB, MIDI, RF, CPU deadlines and listening still
require hardware validation.

The final pi32v2 app uses 482,268 bytes of its 581,564-byte slot, leaving 99,296.
Static RAM sections total 354,144 bytes: globals 46,656, engine pool 304,704 and
RAM code 2,784. The pool retains 39,360 bytes within its reserved region. These
linker measurements do not establish CPU deadlines or stack high-water marks.

## 0.2.0 historical validation

The project now contains 64 patterns × 4 parts × 16 steps. The version 5 portable
record is 34,850 bytes; its in-memory project is 35,214 bytes. Tests cover all 64
queued bar transitions, empty new patterns, recording and automation at pattern 64,
addresses through 319, malformed final-pattern data and unchanged destinations on
failed decode. Existing preset/patch snapshots still match at the explicit previous
master setting; demo arrangements and instrument parameters are unchanged.

Two 36 KiB banks occupy only `0x99000..0xAAFFF`. There is no automatic migration
of development-version flash saves. Storage tests exercise 460 erase, page-program
and header-commit interruption points, read failures, verification failures, range
guards and sequence rollover. A second integration suite checks 280 sampled cuts.
The installer simulation checks preservation of both the old 8 KiB and new 72 KiB
regions. No physical flash operation has been performed.

The final output gain is six times the previous setting. A fixed-point stereo-linked
limiter has a 30,000-sample ceiling, instantaneous attack and approximately 50 ms
release time constant. Tests cover unity transparency, stereo ratios, extreme inputs,
silence, reset, block independence and dense polyphony with all track/master effects.
The hot test reaches the ceiling without clipping. At native default gain, NIGHT
CIRCUIT measures RMS −21.408 dBFS and NEON SPIRAL −19.943 dBFS, about +15.57 dB
relative to the prior master setting, with no limiting in either default demo.
Web master defaults to full scale and shares the firmware's quadratic control map.
These are PCM measurements, not loudness-weighted LUFS or hardware listening results.

The recording metronome is configured with GROOVE knob 4 and saved in the project.
Native and WASM tests verify quarter-note timing, first-beat accent, swing, immediate
stop, and silence unless PLAY, REC and METRO are all enabled. Clicks bypass track
mute/effects and do not write notes or automation. ON/OFF project round trips,
invalid serialized settings and variable audio block sizes are covered.

The final pi32v2 build uses 471,876 bytes of the 581,564-byte app slot (109,688 free).
Static RAM sections total 348,028 bytes: 42,032 globals, 303,212 pool and 2,784 RAM
code. The pool has 40,852 bytes free within its region. Hardware CPU deadlines,
stack high-water marks and listening remain untested.

Current audio and build measurements are in [audio-report.json](audio-report.json),
[showcase-report.json](showcase-report.json), [target-report.json](target-report.json)
and [package-report.json](package-report.json). Listening examples and the MP3 have
been regenerated; older byte-identical audio claims below describe the 0.1.4 release.
Browser operation and real-device audio deadlines remain unverified.

## 0.1.4 historical validation

Host tests pass on the development Mac: 128 distinct presets, eight kits with 128 distinct lane renders,
release/choke/voice allocation, exact swing and bar timing, block-independent DSP,
UI and screen bounds, LA resonance/control regressions, version 1 migration and
version 2/3 migration, version 4 kit/track FX round trips, and 7,842 interrupted storage operations. Unreadable
banks recover safely; saving with an unreadable bank is refused.

Each of four effects is checked with an impulse for changed output, applicable
tail energy, bounded output and eventual decay. The built-in demo is exercised
through completion with zero saturation; all effects at maximum are included in
the 10-second multi-part stress test with all four track processors and master FX
at maximum (peak 28,589, zero clips on host). A separate coherent-voice test at six
tempos deliberately saturates cascaded delay/reverb and verifies the final clamp
without signed overflow. ASan/UBSan passes. Host engine size: 271,708 B;
project: 3,726 B; encoded record: 3,746 B.

The 0.1.4 additions have separate core, UI-input, performance and typed-effects
suites, all run optimized and in fresh ASan/UBSan builds. Input tests cover first
turn, 59/60 ms acceleration boundaries, reversal, context changes, clock rollover,
small-range exclusions, exact 499/500 ms popup expiry and all 128 list strip bounds.
The native HTTP test proves the popup expires while transport is stopped. Pixel
tests require green only for audible unmuted hits in both melodic overview and drum
detail; zero-velocity and muted steps remain non-green. Actual C captures cover
13 screens: ARP off/on/latch, FX amount/type/filter and master scope, preset origin
stars, popup, automation full/cleared and playing home/drums. Visual review found
no clipped labels or overlaps. Selection remains orange and playback green.

Performance tests cover six ARP orders, live key release/latch, generated-note
recording, automation overwrite/capacity/pattern clear and version 4 round trips.
Live ARP and recorded playback PCM match exactly for all three rates at swing 0
and 30. Type tests distinguish all 15 effects, verify delay impulse timing/stereo,
filter frequency response, phaser notch, state clearing and 192 combinations over
five cascaded processors. Legacy DSP impulse hashes at four tempos remain exact.
UI tests cover FX subpages, scoped parameter edits, ARP controls, per-lane levels,
automation recording and origin-name stars. Version 1–3 imports preserve patches;
unknown old modified-patch origins display IMPORTED SOUND.

NEON SPIRAL uses all four native effects. The MP3 is 320 kbps stereo 44.1 kHz,
58.906 seconds including encoding padding; WAV is 58.857 seconds including tails.
Apple AudioFile inspection recognized the MP3. Its provenance and hashes are in
[mp3-report.json](mp3-report.json); raw/normalized renders are in
[showcase-report.json](showcase-report.json). No external musical effects were used.

[CI 38512065587](https://github.com/dspaudio/mc-1/actions/runs/38512065587) passed
host, pi32v2 target, SDK hash checks and sanitized installer simulation at
`9e166c68396b4b6a5f4990266b66393450f9ea3f`, before the factory-bank expansion.
For the 48 presets and twelve PCM attacks at
`f41e6728cdbca74e10009dc11ced1ec388c4826a`,
[CI 38514033913](https://github.com/dspaudio/mc-1/actions/runs/38514033913)
passed host checks, target memory guards and package generation, then failed on
stale checked-in target-report bytes/hash; its installer test was not reached.
The report repair was verified by a local Docker pi32v2 rebuild reproducing that
run's app and package bytes on 2026-10-14, then by
[CI 38523690699](https://github.com/dspaudio/mc-1/actions/runs/38523690699)
at `57e6b55`, which passed both jobs.

The current reports below include the expanded bank, kits and per-track FX as well
as home-grid pitch marks and GLO global mixer. Local host tests prove all four demo patterns advance and change
the rendered melodic grid, including chords. The home PATTERN/NEXT status is
checked across queued bar transitions on both melodic and drum pages, with
save/load message priority and no overlap of the sixteenth drum row.
Mixer checks cover independent
part levels, clamps, audible output, mute indicators, the mixed-case
`SELECT-ARP to mute` hint, project serialization and
the mixer → groove → demo → automation page cycle. The clean ASan/UBSan binary, host render,
native HTTP checks, all 129 audio renders, pi32v2 memory guards, package build and
sanitized installer simulation passed. No hardware timing result is implied.
Compiler installation paths differ between local Docker and CI; the checked-in
target report retains the CI compiler path.

| Target region | Used | Bound | Remaining |
|---|---:|---:|---:|
| App load image | 469,896 B | 581,564 B | 111,668 B |
| Data + BSS | 14,672 B | 98,304 B | 83,632 B |
| RAM code | 2,784 B | 24,576 B | 21,792 B |
| Pool | 271,708 B | 344,064 B | 72,356 B |

The app SHA-256 is `f85711197fdc6ceff0288e54198f09ba8e8087bd54aa870976f38ae163e660b0`.
The experimental package is 610,019 B, SHA-256
`976db107928f37ae2b10abb379ccb2c3020b90f115a8d3de58c527846d2be168`.
The RAM loader is 8,140 B, BSS end `0x01C0F3A0`. Static app RAM above sums to
289,164 B excluding stacks/reservations; no measured stack high-water is claimed.

Each kit passes all 16 non-silent, distinct-output, decay and hat-choke checks;
changing the selected kit leaves existing drum voices unchanged. The 64-second
[kit audition](audio/kit-showcase.wav) plays eight seconds per kit in list order.
All 64 presets from 0.1.2 retain their names, parameters and PCM hashes after matching
by name across the reordered bank. Factory demo output and all six existing listening
WAVs, including NEON SPIRAL and the kit comparison, are byte-identical to 0.1.2.
Independent frozen fixtures check the old 64 patch/PCM results and both built-in
demo project records. All 129 current renders pass the unchanged energy, DC,
clipping and release checks.

The 64 new patches use all 16 CC0 instrument recordings, with PCM mix 127. Tests
change oscillator waveforms while confirming identical PCM-only output, then verify
that lowering PCM mix audibly changes the rendered samples. All 112 mip tables stay
within ±16,000 and valid interpolation/loop bounds. Eighty integration cases exercise
MIDI notes 12, 36, 60, 84 and 108 across all sources, block boundaries, held loops and
release. Independent sanitizer coverage checks all 97 supported notes and fractional
sample ends. New patches round-trip through project version 4; invalid source IDs and
recorded sources assigned to Acid are rejected. Earlier saves load with their original
patches; earlier firmware cannot load new recorded-source IDs.

Offline regeneration verifies the retained WAV and CC0 license hashes and produces
byte-identical PCM. Source commits, SFZ keycenters and conversion settings are in the
[sample manifest](../assets/recorded/manifest.json). The conversion uses 128-tap Blackman
resampling at 90% of Nyquist, 63-tap half-rate mip filtering, 2 ms onset margin, a
2 ms fade-in and 64 ms end fade or sustain-loop crossfade. Each base excerpt is
0.384 seconds at 16 kHz; looped instruments repeat 0.192–0.384 seconds. These are
compact single-note, single-velocity recordings, not full multisampled instruments.
All measured loop wrap jumps remain below each table's maximum adjacent jump;
this is a numeric discontinuity check, not a listening-quality guarantee. Pitch roots
follow the upstream SFZ mapping. Near-root autocorrelation estimates agree within
16.2 cents for 15 recordings; the inharmonic glockenspiel estimate differs by 68.3
cents, so no pitch correction was inferred or applied.

The [48-second recorded audition](audio/recorded-showcase.wav) plays one preset per
source at MIDI note 60 in manifest order, three seconds each, with no external effects.
An eight-voice recorded-PCM stress render with all track and master FX takes 0.061
seconds of host CPU for ten seconds of audio in the optimized Mac build. This does
not establish target CPU load, codec sound, IRQ latency or hardware stability.

The generator validates 128 unique names, patches and symbolic IDs, and 14 contiguous
category ranges. Tests browse all 128 sounds on each melodic part, wrap in both
directions and resume from the actual patch after project loading. Actual C frame
captures cover the first preset, Acid boundary, Mallet/Bell, long Hit/Percussion
category, last preset 128 and both demos; labels and number fit above the grid.

Track effects are tested across all 16 part/effect combinations for changed output,
isolation from other parts, muted-tail suppression and clearing buffers on dry bypass.
UI checks cover pattern/sound encoder roles, kit wrap, selected-track/master FX scope,
and saving all 16 amounts. Valid-CRC out-of-range kit/FX records are rejected without
changing the destination. Version 1/2 imports default to kit 0 and dry track FX.
Actual C frame captures of the first/last kits, edit mode, melodic/drum FX and master
FX were visually checked for labels and overlap. No browser was used for this revision.

Apple's MP3 decoder produced 44.1 kHz stereo PCM with peak 23,731 and zero saturated
samples. The public MP3 was fetched and verified against the local SHA-256.
During the earlier effects/demo revision, the in-app browser displayed the real C demo library, loaded NEON SPIRAL and showed
its four effect defaults (48/35/20/30).
See [source audit](AUDIT.md) and [build/recovery boundaries](../BUILDING.md).

No FM-1 has been flashed or accessed. Installation, cold/warm boot, physical panel,
codec sound, actual flash writes, clock, stack usage, worst-case audio deadline,
UBOOT recovery and long-duration stability remain unverified. The audio app has no
USB updater; an actual device trial needs a known recovery path and separate permission.
