# step velocity (OS 1.2.1): RE notes (read-only, static)

tools here: `a.sh lo hi` (annotated disasm, hex without 0x), `d.sh lo hi` (raw), `rd.py addr nwords` (read stored data blocks).
caveat: objdump hides Blackfin+ 64-bit loads/calls; `a.sh` decodes them. Lines with `|| NOP` are real instructions.

## key numbers (name table 0x840acac8, 72 entries; verified by dumping the KName strings)
0 Help 1 Tempo 2 Synth 3 Drum 4 Tape 5 Mixer | 6-9 Track1-4 (= M1..M4) | 10-13 EncClick1-4
14-21 KSound1..8 = the 8 TRACK buttons (instrument 1-8) | 22 Seq 23 Mic 24 Usb 25 Bar
26-49 keyboard (F3..E5) | 50 Rec 51 Play 52 Stop 53 Left 54 Right 55 Shift | 56-71 KStep1..16
step key -> index: 8d5b864e(key) = key-56 if 56..71 else -1.   keyboard test: 8d5b8814 (key-26 < 24).

## key flow
queue consumer loop 8c0b6104..: entry (16 B: key, pressed) -> 8c0b6184 CALL 8c0b5d58(entry) (SDK hook site) -> 8c0b5ff6(queue).
8c0b5d58 (input enabled byte 08023c48): encoder clicks 10..13 special; press: shift ? 8c0b5b80 : 8c0b5b10 (screen vtable
+0x10/+0xc via 8c0944b0(); when it returns 0 falls to global handlers 8c092628 (shift) / 8c092304 (plain)), then 8d5b8100(key)
adds key to the held list. release: 8d5b8030(key) removes from list, then 8c0b5bbc (vtable +0x14, fallback 8c092e20).
global object (the `this`): 0x0801ebec.   held-key list: base 0x86bda0d4, count word +0x180, stride 12, key in word 0.
8d5b7d7c(key) = is held.  8d5b7f40(key, ms) = held longer than ms.

## held step state
word 0x86bda0c8 = held step index 0..15, or -1.  8d5b7e34() = (word != -1).  8d5b7e4c() = the word.
set by 8d5b7e60(step) (only when -1), from step-down 8c09171c(this, step); cleared by 8d5b7e08(step) in step-up 8c092c20 (8c092cd0).
absolute step in the pattern = 8d5b7e4c() + 16 * 8d585fb4(track)  (page). SDK already does this: lock_handles.cpp held_slot().
SDK `Control::held` (PR 414, held_with) scans keys <56 only: it never sees a step key. Need a new field.

## stock behaviour of step + track button (NOT confirmed)
- track button DOWN (key 14..21): jump table 0x8400b748 (keys 0-25; 26-49 fill 8c09234c; 50-55 at 0x8400b810) -> 8c0924c8.. -> 8c091f18(this, idx).
  no step-held check in it: handles Synth/Drum held (8c091144), shift, "other track button held 200 ms" (range, [this+0x18]),
  else sets [this+0x1c] = (idx == selected track), 8c091d6c, 8d592998.
- track button UP: key-up table 0x8400b904 (keys 2..25) -> 8c090e20(this, idx). If byte 0x8400b60b is set it clears it and
  returns (stock's "swallow next release" flag); else switches to page 0x24 (8c094368) / 8c094178(...). Also no step-held check.
=> static reading says stock does NOT special-case step+track button. User saw "nothing": maybe pressed the already selected
   track. MUST be checked on device: does the selected track change on release?
- STEP release (8c092c20): if per-step press flag B[this+step] != 0 it is a TAP: no keyboard keys held -> 8c0911b0/8d582218 checks
  -> 8d592630(0x860c6adc, abs step) (call at 8c092dcc) adds the default note (or toggles). If any edit consumed the press, flag is 0.
  stock "press consumed" = 8c0905bc(this): if step held { B[this+heldstep]=0; B[this+0x1e]=0 } (called from the encoder path 8c0b5c80 with
  this = 0x0801ebec). Our edit MUST call it, else releasing the step afterwards would toggle/remove the note.

## step entry (tap)
8d592630(controller=0x860c6adc, abs step): [P5 = selected track B[0x840a0534]]
  A) if B[ctrl+0x304d+step]!=0 or [ctrl+0x8424]/[ctrl+0x8428] mismatch -> 8d59271e: pitch = 8d4d6c98(track) (track's key),
     call 8d5925b0(ctrl, 480*step, pitch, vel=100, len=240)  [CALL at 8d592742, bytes ffe337ff, file off 0x1c999e]
     velocity literal: `P1 = 0x64` at 8d592734 (29e16400, off 0x1c9990); length `P1 = 0xf0` at 8d592724.
  B) else: seq object 8d5ab816(ctrl+0x108+0x290*track), if 8d5adde4(...) finds no note and count (W[+0x4d02]) != 120: builds Note on stack
     {pos 480*step, len 240, pitch 8d4d6c98, vel 100 (`R0 = 0x64` at 8d5926e0, 20e16400, off 0x1c993c), flags 0}
     CALL 8d58f24c(container = obj + 0x457c, &note, -1)  [at 8d5926fa, ffe3a9e5, off 0x1c9956]; then 8d587668(track) publish; 8d591930 undo bookkeeping.
  8d5925b0(controller, position, pitch, velocity, length): arg4 velocity and arg5 length are on the stack ([FP+0x14],[FP+0x18]); builds the
  12-byte Note, 8d5875c8(track,1) container, count check 120, CALL 8d58f24c(container, note, -1) at 8d59260c, 8d587668(track), 8d591948.
  other callers of 8d5925b0: 8c0922d6 (abs; keyboard key while step held: velocity = played velocity, length = hold time, DON'T touch),
  8d594412 (rel; live note on while step held: played velocity, len 240, DON'T touch), 8c092d66 (abs, bytes 00de598db0250000, off 0xc9fa2:
  chord tap = keyboard keys held while tapping a step: vel `P3 = 0x64` at 8c092caa, len 240).
  other callers of 8d592630: 8c0918ee, 8c091b1e, 8c091d4e (Left/Right page code; also reach the same default note).
=> stock default velocity for step-entered notes: 100 (0x64), length 240 ticks (half a 480-tick step).

## container / positions
pattern container for track t = [0x840a0524] + 0xd7c + t*0x53f44 + pattern*0x53f4 + 0x457c = 8d5875c8(t, x) (both branches).
Slot {i16 prev, next; Note{i32 pos; u32 len; u8 pitch, velocity, flags, pad}} (notes::Container in note_container.hpp).
step of a note = floor((pos + 240) / 480) at EVERY scale (8c0920ec: *0x88888889>>8); window for step s = [480*s-240, 480*s+240).
length of a pattern = 480 * 8d586800(track).  publish = 8d587668(track).  undo begin (as extension_write): 8d591930(0x860c6adc); 8d593c18(0x860c6adc, track).
popup: 8d549168(text, 1, 0, 0x3f000000) (= Actions::notify).

## COM > M1 > system menu (1.2.1) for the default-velocity row
init in the com controller static ctor 8c098d60..8c099080. six rows: brightness(0x8400bf9c) led brightness(bfa8) country(bfb8)
power off(bfc0) sample preview(bfcc) key velocity(bfdc). vector object (alloc,begin,end,cap) at 0x0801eee0 (6 rows), next menu 0x0801eef0 (5 rows)
(in 1.2.0 it was 0x0801f298 with the rows built at 8c099848). Row pieces: `new` 24 bytes = 8d4d2900(0x18), Parameter ctor 8c089110
(obj, name, default, step, max) e.g. brightness: name 0x8400bf9c, default 0x32, step 0xa, max 0x64; led brightness default 0x28, step 1, max 0x40.
vector fill helper 8c097f0c(vec, rows, n). callbacks (format, set) pairs seen: 8c098ad0/8c097b2c, 8c097b3a/8c097b58, 8c097b76/8c097b94, 8c097baa/8c097bde.
Stock persists a named Parameter via its settings db (ctl_knob.cpp "db_key_"), PR 61 used a null name = no persistence (reboot scoped).
NOT ported yet: delete (8d4d32b4 in 1.2.0), string assign for format (8c0418f0 in 1.2.0), Parameter vtable (0x0803f61c in 1.2.0).

## proposed hook sites (all existing style: profile `hook`, call:true, service extensions)
| address | len | before | file off | symbol |
| 0x8d592742 | 4 | ffe337ff | 0x1c999e | sdk_extensions_step_note |
| 0x8c092d66 | 8 | 00de598db0250000 | 0xc9fa2 | sdk_extensions_step_note |
| 0x8d5926fa | 4 | ffe3a9e5 | 0x1c9956 | sdk_extensions_step_insert |
validated my address -> file offset method against the existing live_note_on hook (0x8d594376 -> 0x1cb5d2, 00e3e109): matches.
