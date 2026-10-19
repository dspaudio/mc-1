# MIDI and USB Audio

MC-1 1.0.0 connects the shared sequencer and synth to USB MIDI and the FM-1 TRS
MIDI input. The USB device also exposes a stereo USB Audio capture stream. These
are firmware implementations; physical enumeration, electrical MIDI input and
audio deadlines still require FM-1 testing.

## Connections

| Connection | Direction | Implementation |
|---|---|---|
| USB MIDI | Input and output | Computer is USB host; MC-1 is a USB MIDI device |
| TRS MIDI | Input | UART1, PH8 input, 31,250 baud; no TRS output driver |
| USB Audio | MC-1 to computer | UAC1, stereo signed 16-bit PCM, 44,100 Hz |
| Web MIDI | Input and output | Explicit browser permission and selectable OS MIDI ports |
| FM-1 BLE MIDI | Unavailable | Radio/controller/runtime integration is not implemented |

USB Audio carries the final mix after effects, master level and peak limiting.
It does not accept computer audio into the synth. USB keyboard hosting is not
implemented: a USB MIDI keyboard and MC-1 are both devices and need a host.

## Settings

GLO opens the MIDI page after the other global pages. The four knobs set the
four part channels; displayed channels are 1–16. LFO selects the clock source.
Channel and clock settings are saved with the project. The default channels
are 1, 2, 3 and 10. Each transport has independent MIDI parser state, so bytes
from USB and TRS cannot complete each other's messages.

In the web emulator, enable audio, connect MIDI and choose input/output ports.
An OS-paired BLE MIDI interface may appear in that list, depending on the OS and
browser. This uses the computer's Bluetooth stack and does not activate the
FM-1's Bluetooth radio. The emulator presents Web MIDI input as the USB source.

## Messages

Note On/Off and velocity address the configured part channel. Melodic notes use
the engine's 12–108 range. PLAY + REC writes incoming notes to the current step;
an enabled arpeggiator records its generated notes. Program Change 0–127 selects
the melodic bank; 0–7 on the drum channel selects a kit. Pitch Bend has a fixed
approximately ±2-semitone range. Channel pressure and polyphonic pressure have
no synth destination.

| CC | Destination |
|---|---|
| 5 | Glide |
| 7 / 10 | Track level / pan |
| 74 / 71 | Cutoff / resonance |
| 73 / 75 / 70 / 72 | Attack / decay / sustain / release |
| 91 / 94 | Selected channel's R/D1 send (94 is an input alias) |
| 93 | Selected channel's C/F1 send |
| 92 / 95 | Selected channel's R/D2 / C/F2 send |
| 20 / 23 | Global R/D1 / C/F1 type, 0–7 |
| 21 / 24 | Global R/D2 / C/F2 type, 0–7 |
| 64 | Sustain pedal |
| 120 / 123 | All Sound Off / All Notes Off |

The four sends range from 0 to 127. Outgoing send edits use CC 91, 93, 92 and 95
on the part's configured channel. Type edits received on any configured part channel
select the same global bank; outgoing type edits use part 1's configured channel.
Types use enumeration values 0–7, not a scaled 0–127 bank; larger values are
ignored. CC 21 and 24 now select the second bank pair; hosts sending their
formerly inactive legacy controls should remove those mappings. Legacy drive
and filter CCs 12, 22 and 25 remain inactive. Other unmapped controls are ignored. Drum lanes map to MIDI notes
`36, 38, 42, 46, 39, 45, 35, 37, 40, 50, 49, 51, 70, 56, 75, 47` in panel order.

The selected external clock source accepts Clock (24 pulses per quarter note),
Start, Continue and Stop. Other sources can still send notes but cannot steer
transport. Internal clock generates MIDI clock and transport output. Incoming
messages are not echoed back; sequencer and arpeggiator performance generate
their own output. Song Position Pointer, MIDI Time Code and MIDI file import are
not implemented. Bounded output queues recover from overflow with an all-channel
All Notes Off sequence instead of leaving unbounded pending messages.

저장하는 동안에는 USB/TRS 연주 입력을 받지 않고 기존 입력 음을 해제한다.
저장이 끝나면 UART DMA와 파서를 초기화하고 다시 수신한다. 저장 중 누른 건반은
재개 후 다시 눌러야 한다. 실패한 저장에서도 같은 수신 재개 절차를 거친다.

데모를 교체할 때는 이전 MIDI 발음 정리를 새 연주보다 먼저 출력한다.
MC-1이 실제로 Note On을 출력한 채널에만 sustain 해제, All Sound Off,
All Notes Off를 보내며, 사용하지 않은 채널에는 데모 교체로 인한 정리 메시지를
보내지 않는다. 출력이 밀려도 이 정리와 새 연주의 순서를 유지한다.

## BLE integration boundary

FM-1's chip has Bluetooth hardware. The missing component is its integration
into this firmware, not the radio capability itself. The current bare-metal
application links startup, interrupt vectors, ISR support and the application;
it does not run the official SDK's Bluetooth tasks or RF initialization.

The official SDK at commit
[`d179b4484759423312073f5fbb232501aa491047`](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/tree/d179b4484759423312073f5fbb232501aa491047)
shows the required pieces in `apps/demo/demo_ble/bt_ble/ble.c` (Bluetooth stack
initialization and the initialized event), `apps/demo/demo_ble/app_main.c`
(controller and stack tasks), and `apps/demo/demo_ble/board/wl82/Makefile`
(system, RF, controller, host stack and crypto libraries). Its board configuration
does not establish compatibility with MC-1's 1 MiB flash / 512 KiB RAM layout.
An actual port must resolve that runtime, linker layout, controller timing and
RF setup before a BLE MIDI GATT service can be advertised and tested.

The inherited ORBIT/SLOOP driver also explicitly leaves the radio off. Source:
[`README-SLOOP.md` at 07e3fed](https://github.com/dspaudio/orbit/blob/07e3fed30e8e6757dbe3c6810276599987e4970e/README-SLOOP.md).
No Bluetooth binaries, proprietary ROMs or placeholder radio driver are bundled.
