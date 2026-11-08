# step velocity: spec

an open-xy firmware extension for op-xy os 1.2.1. background and addresses: `step-velocity-dig-notes.md`.

## behaviour (decided)
- hold a step key, press track button 1–8: every note on that step (chords too) gets velocity 4, 8, 16, 32, 64, 100, 112, 127 (the velocity step component's values).
- while a step is held, the track buttons don't select tracks. stock does switch tracks on that combo; giving that up is fine.
- releasing the step after an edit must not add or remove a note (call stock's consume `8c0905bc(0x0801ebec)`, as encoder turns do).
- feedback: while a step is held, the track-button leds show only velocity: all eight off when the step is at the default 100 (or empty), otherwise only the button matching its velocity lit, in pink (red + white, start with `0x7f80`; one named constant in the extension so the tint is easy to tune) (exact match, else the nearest table value). stock's lights (selected track etc.) come back on release. so a lit button never means "default", and the selected-track light can't be mistaken for a velocity. no popup needed.
- one undo snapshot per changed edit; pressing the button for the value it already has changes nothing and takes no undo.
- aux tracks and empty steps: nothing happens (the track buttons still don't switch tracks while a step is held, for consistency).
- tapped steps keep stock's default velocity 100. no setting, no step-entry hook.

## where
- sdk: the open-xy sdk, branch `sdk/step-velocity` from `sdk/bar-loop` (3258b10, the bar tools sdk pr etcetc000/open-xy#414, still open). stacked on it because both change `Control` and the key hook.
- extension: `extensions/step_velocity` in this repo (same layout as bar tools' `extensions/bar_tools` and capture).
- stock dumps for reference (not committed): a local `local/` folder (`blk/`, `dis_*.txt`, `consts.pkl`, `ann.py`).

## pieces
### A. sdk: held step, consume, velocity edit
1. `keys::sound1..sound8` = 14..21, `keys::step1` = 56, named like the existing `keys::`.
2. `Control::step` (i32: absolute held step on the selected track, `8d5b7e4c() + 16 * 8d585fb4(track)`, −1 when none; held test `8d5b7e34()`) and `Control::track` (u32 selected track), snapshotted before stock handles the key, like `held`/`shift`.
3. pre-stock consume: a claim for the track buttons while a step is held. if the claims registry is strictly one key per claim, use eight (`held_step_track_1..8`); otherwise one group claim. when the owner takes a press, stock never sees that press or its release (the sdk remembers taken keys). document the change to the `queued_control` contract in `docs/extensions.md`.
4. action `edit_velocity`, unlocking:
   - `Actions::set_step_velocity(const Context&, u32 track, u32 step, u32 velocity)` → notes changed. requires a current context and an instrument track; validates the list with `notes::valid`; selects notes with `floor((pos + 240) / 480) == step`; returns 0 with no undo if nothing changes; else one undo (`8d591930` then `8d593c18(0x860c6adc, track)`), set velocity, publish `8d587668(track)`, consume the step press `8c0905bc(0x0801ebec)`.
   - `Actions::step_velocity(const Context&, u32 track, u32 step)` → velocity of the step's first note, or −1. read only.
   - reuse `extension_write`'s container, validation, undo and publish code; don't duplicate it.
5. `docs/abi-1.2.1.md` and `docs/extensions.md` updated for every new address and interface.
- done: host tests with stock doubles, in the style of the existing extension tests, cover: field snapshots (step held or not, page > 0, shift), consume of press + release, chord edit, other steps untouched, same value → 0 and no undo, held-flag notes, stale context and track ≥ 8 refused, consume call made. `python3 sdk.py check` passes.

### B. sdk: track-button led feedback (research done: leds-dig-notes.md)
- stock rebuilds a u16[72] led buffer every main-loop frame (low byte white 0-255, bits 8-14 red, bit 15 blink; track buttons = 14..21) and only sends changes. so an override just overwrites buf[14..21] after stock's last painter; stock's lights return on their own when it stops.
- new hook (kind hook, call true, service extensions): `0x8c0bab32`, 8 bytes `00de588de4b00000`, file offset `0xf1d6e` = `CALL 8d58b0e4(0x860bf4b4, buf)`, the keyboard-note painter. wrapper `sdk_extensions_key_lights(u32 obj, u16* buf)` calls stock, then the extensions.
- interface: `Hooks::track_lights` / `with_track_lights(...)`, `struct TrackLights { bool take; u16 light[8]; }` (stock's full led word: white low byte, red bits 8-14; blink bit 15 masked off), `void(const Context&, i32 held_step, TrackLights&)`, called every frame with take=false; if taken, `buf[14+i] = light[i] & 0x7fff`. new permission `light_tracks`. last taker wins.
- done: fixture test that buf[14..21] is overwritten only when taken and the rest of the buffer is untouched; abi doc hook count and address.
### C. extension
- `extension.json` (claims, actions `edit_velocity`), `extension.cpp`, tests with a fake `Actions` (as capture's `gesture.cpp`), `README.md`, `QA.md` (device checklist below).

## device qa
1. hold step, press track 1..8: velocity changes (check in stock's note view or by ear); led follows, pink (does red + white actually read as pink? tune the constant if not).
2. chord step: all notes change.
3. release the step afterwards: no note added or removed.
4. selected track unchanged.
5. one undo restores the old velocities; repeating the same button adds no undo.
6. pages 2–4 and a non-1x scale.
7. aux track selected: nothing changes.
8. tap an empty step: note at 100, as stock.
9. keyboard key while holding a step: played velocity, as stock.
10. a velocity step component on the step still wins at playback.
11. bar tools and capture still work.

## pink party (photo mode)
- toggle: hold an **empty** step, then press track 1 and track 8 together (the second while the first is still down). same gesture turns it off. on an empty step the buttons edit nothing, so the gesture can't change notes. off after reboot.
- while on: every key led on the keyboard is pink (`pink`, all 72 indices), every frame, whatever else is happening. the step velocity leds are hidden while it's on.
- sdk (branch `sdk/step-velocity`, next to the track lights hook, same hook site `0x8c0bab32`, no new patch site): `struct KeyLights { bool take; u16 light[72]; }`, `using OnKeyLights = void (*)(const Context&, KeyLights&);`, `Hooks::key_lights` + `with_key_lights`, permission `light_keys` (action, no requires, like `light_tracks`). runs after `track_lights`; if taken, `buf[i] = light[i] & 0x7fff` for all 72; last taker wins. docs + fixture tests in the style of the track lights ones.
- the extension tracks which track buttons are down itself from its taken press/release events (stock never sees them, so stock's held list doesn't).
