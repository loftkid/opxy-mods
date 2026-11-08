# key led path (OS 1.2.1), static re

## reuse search
open-xy (upstream/main, upstream/staging, all branches grep), issues/prs "led"/"light" (gh), open-xy-core-machines code search: nothing on key leds.
only hits are unrelated (brightness in docs). so this is first-hand re.

## how a key led is driven
- 72 key leds, index = key number (0..71, same table as the notes: 14..21 = track buttons, 56..71 = steps).
- led value = u16: low byte = white level 0..255, bits 8..14 = red level 0..127, bit 15 = blink (on/off by phase).
  stock uses 0x00ff / 0x0099 / 0x0066 (white), 0x7f00 / 0x4c00 (red), 0 off.
- leds sit on the keyboard mcu ("PSOC"), reached by a serial message queue: send = 0x11a00ec0(ctx 0x8418599c+4 style, cmd, data, len..).
  strings: "kb white current", "kb red current", "led current tuning" (diag), "led brightness"/"led_brightness" (com setting, cmd 0x6b via 8d5b25b0).
- set one key led: 8d5b8b12(key R0, value R1). dedupes against a cache u16[72] at 0x86bdaaa4: same value = no message. else
  cache = value; message cmd 0x40, 4 bytes {0, key, (value & 0x7f00) >> 7, value & 0xff}.
  clear all: 8d5b8b8c (cache zeroed, cmd 0x40 {1}). callers of 8d5b8b12: ONLY 8c0bab82 (the frame loop) and 8c08cc14 (random led test, diag screen).
- other led senders (not key leds): 8d5b8a30(a,b,c) cmd 0x40 {3,a,b,c} (3-channel single led, cached 0x86bdaa94..96), 8d5b8ac0(x) cmd 0x40 {2,x,x}
  (callers 8c0b6ee2 init, 8c0baa72 every 5th frame, 8c08cbc0 test).

## the frame (repaint every main-loop iteration)
main loop calls 8c0baacc (-> 8c0baad0) at 8c0bacf6 once per iteration:
  8c0baad6 8d5b7dc0(); 8c0baade 8d580074();
  8c0baaf0 CALL 8d5986b8(0x860c6adc, elapsed)   <- the sdk's existing controller_update hook (BEFORE the led buffer is built)
  8c0baafe memset(buf=0x08034d7c, 0, 144)       72 x u16
  8c0bab1e CALL 8c090608(this=0x0801ebec, buf)  screen/mode painter: step keys, track buttons, etc. (big, many branches)
  8c0bab32 CALL 8d58b0e4(0x860bf4b4, buf)       keyboard note leds, last painter
  8c0bab3a.. loop i=0..71: v=buf[i]; if bit15: v = (blink phase) ? v : 0 (phase = bit0 of a counter derived from 0x8400f3d0);
             8c0bab82 CALL 8d5b8b12(i, v)
  then every 5th call: 8c0baa0c (other led, 8d5b8ac0)
=> stock rebuilds ALL key leds from scratch every iteration; only changes go out on the bus (cache dedupe).
   an override must be applied to buf every iteration (after 8c090608, before the loop). nothing to "suppress": overwriting buf[14..21]
   wins, and when the override stops the next frame yields stock values that differ from the cache, so stock's lights return by themselves.
   no restore code needed. (the existing 0x8c0baaf0 hook is too early: memset runs after it.)

## where stock paints the track buttons (inside 8c090608)
- selected track: 8c0908a0..8c0908b6: idx = (B[0x840a0534] & 7) + 14; buf[idx] = (sel > 7 ? 0x7f00 : 0x00ff).
- track-button group loops write buf[14..21] (P4 = buf + 0x1c): 0x0099 white / 0x4c00 red when sel >= 8 (8c09095a..), 0x0066 (8c0908c2..).
  all modes' variants end in buf; irrelevant once we overwrite after.

## hook site
| address | len | before | file off | note |
| 0x8c0bab32 | 8 | 00de588de4b00000 | 0xf1d6e | CALL 8d58b0e4(0x860bf4b4, buf): R0 = 0x860bf4b4, R1 = buf (loaded at 8c0bab2a / 8c0bab22) |
verified in blk/8c03d000.bin and the stock os.ldr (same method as the 8c0baaf0 hook: 0xf1d2c, 00de598db8860000).
wrapper: `void sdk_extensions_key_lights(u32 obj, u16* buf) { stock<void(u32,u16*)>(0x8d58b0e4)(obj, buf); extensions_key_lights(buf); }`
(return value of 8d58b0e4 is unused: R0 is zeroed at 8c0bab6e.) same shape as sdk_extensions_controller_update.

## proposed sdk surface
Hooks member (not an Action: lights are a per-frame state, and a one-shot action would be repainted away next frame):
    struct TrackLights { bool take; u8 level[8]; };   // white level 0 (off)..255 per track button 1..8
    using OnTrackLights = void (*)(const Context&, i32 held_step, TrackLights&);
    // Hooks::track_lights, builder with_track_lights.
SDK: every frame after stock's painters it calls each extension's callback with take=false (and held_step =
extension_held_step(selected track), -1 none). if take: buf[14+i] = level[i] (white only: red=0, no blink). several extensions: last taker wins,
or only extensions with a claim/permission `light_tracks` (profiles/extensions.json) are called.
For this feature: callback returns take=true while held_step >= 0 (all 8 level 0, except the button matching the step's velocity = 0xff);
step at 100 or empty: all zero. released: take=false, stock lights return next frame.
Needs a new permission (`light_tracks`) and a doc line in abi-1.2.1.md ("eight hooks"). host fixture: a fake buf, check overwrite of 14..21 only
and untouched otherwise.
The callback runs every frame, so it must be cheap (no allocation); the held step can be read via 8d5b7e34/8d5b7e4c as the sdk already does.

## unconfirmed / risks
- main-loop rate not measured (it is every iteration; bus traffic is bounded by the cache dedupe).
- the 16-bit format was read from disasm (masks 0x7f00 / 0xff, wire bytes); whether white 0xff vs 0x99 differ visibly on device is untested.
- which mode branch of 8c090608 runs with a step held was not traced (not needed with a post-paint override).
- 8c0baa0c (every 5th frame) and 8d5b8a30 drive other leds (not 14..21 as far as read); not fully traced.
- the 8d58b0e4 function boundary looks odd in objdump (starts at "P1=R0; LINK"); call args R0=0x860bf4b4, R1=buf are certain from the loads.
- could not test on a device (read-only).
