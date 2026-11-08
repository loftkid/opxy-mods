#include <opxy/extension.hpp>
#include "policy.hpp"

// Hold a step key and press a track button (1-8): every note on that step
// gets velocity 4, 8, 16, 32, 64, 100, 112 or 127. While a step is held the
// track buttons are taken (stock would switch tracks), on any track, so the
// selected track never changes; aux tracks and empty steps just do nothing.
// The track buttons' leds show the held step's velocity (policy.hpp).
// Pink party: hold an empty step and press track buttons 1 and 8 together to
// toggle every key led pink (off after reboot).
namespace opxy { namespace extensions { namespace step_velocity {
namespace {
Party party = {};

// Take every track-button press while a step is held; its release follows the
// press, so stock sees neither.
bool take_control(const Context&, const Control& control) {
    return is_track_button(control.key) && control.step >= 0;
}
void queued_control(const Context& context, const Control& control, const Actions& actions) {
    if (!is_track_button(control.key)) return;
    track_button(party, control.key, control.pressed);
    if (!control.pressed || control.step < 0 || control.track >= 8) return;
    // The step is empty (nothing to read), so the buttons edit nothing below.
    if (party_gesture(party, control.key) &&
        actions.step_velocity(context, control.track, static_cast<u32>(control.step)) == -1)
        party.on = !party.on;
    actions.set_step_velocity(context, control.track, static_cast<u32>(control.step), velocity_for(control.key));
}

// Every frame: while a step is held, the track buttons show its velocity.
// The callback's actions are read only (step_velocity).
void track_lights(const Context& context, i32 held_step, const Actions& actions, TrackLights& lights) {
    lights.take = held_step >= 0 && !party.on;   // the party already covers the buttons
    if (!lights.take) return;
    const u32 track = context.recording.track;
    const i32 velocity = track < 8 ? actions.step_velocity(context, track, static_cast<u32>(held_step)) : -1;
    light_words(track, velocity, lights.light);
}

// Every frame: all 72 keys pink while the party is on.
void key_lights(const Context&, KeyLights& lights) {
    lights.take = party.on;
    if (lights.take) party_words(lights.light);
}
}
extern constexpr Hooks hooks = Hooks()
    .with_take_control(take_control)
    .with_queued_control(queued_control)
    .with_track_lights(track_lights)
    .with_key_lights(key_lights);
}}}
