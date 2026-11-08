// Step velocity driven as the SDK runtime calls it, with fake actions standing
// in for the SDK's velocity edit and read.
#include "extension.cpp"

using namespace opxy;
using namespace opxy::extensions;
#define CHECK(value) do { if (!(value)) return __LINE__; } while (false)

namespace {
u32 edits, last_track, last_step, last_velocity;
u32 insert(const Context&, u32, const Note*, u32) { return 0; }
u32 replace(const Context&, u32, const Note*, u32) { return 0; }
void notify(const char*) {}
bool loop(const Context&, const BarLoop&) { return true; }
bool play(const Context&, u32) { return true; }
u32 set_velocity(const Context&, u32 track, u32 step, u32 velocity) {
    ++edits; last_track = track; last_step = step; last_velocity = velocity;
    return 1;
}
i32 stored = -1;
i32 get_velocity(const Context&, u32, u32) { return stored; }
const Actions actions = {insert, notify, replace, loop, play, set_velocity, get_velocity};

Context context(u32 track = 2) {
    Context c = {};
    c.recording.track = track;
    return c;
}
Control control(u32 key, bool pressed, i32 step, u32 track = 2) {
    return {key, pressed, true, false, 0, keys::none, false, 0, step, track};
}
bool taken(const Control& c) { return step_velocity::hooks.take_control(context(c.track), c); }
void press(const Control& c) { step_velocity::hooks.queued_control(context(c.track), c, actions); }
void release(u32 key, i32 step = -1, u32 track = 2) { press(control(key, false, step, track)); }
bool is_party() { return step_velocity::party.on; }
bool all_pink(const KeyLights& l) { for (u32 i = 0; i < 72; ++i) if (l.light[i] != step_velocity::pink) return false; return true; }
KeyLights keys_lit() {
    KeyLights l = {};
    step_velocity::hooks.key_lights(context(), l);
    return l;
}
bool all_off(const TrackLights& l) { for (u32 i = 0; i < 8; ++i) if (l.light[i]) return false; return true; }
TrackLights lights(i32 held, i32 velocity, u32 track = 2) {
    TrackLights l = {};
    for (u32 i = 0; i < 8; ++i) l.light[i] = 0xffff;
    stored = velocity;
    step_velocity::hooks.track_lights(context(track), held, actions, l);
    return l;
}
// The one lit button, or -1 for none (-2: more than one, or a wrong colour).
i32 lit(const TrackLights& l) {
    i32 found = -1;
    for (u32 i = 0; i < 8; ++i) {
        if (!l.light[i]) continue;
        if (found >= 0 || l.light[i] != step_velocity::pink) return -2;
        found = static_cast<i32>(i);
    }
    return found;
}
}

int main() {
    stored = 100;   // a step with a note: the buttons never start the party here
    // Each track button sets its table value on the held step of the selected track.
    const u32 want[8] = {4, 8, 16, 32, 64, 100, 112, 127};
    for (u32 b = 0; b < 8; ++b) {
        const Control c = control(keys::sound1 + b, true, 37, 3);
        CHECK(taken(c));
        press(c);
        CHECK(edits == b + 1 && last_track == 3 && last_step == 37 && last_velocity == want[b]);
    }
    // Releases are ignored.
    const u32 before = edits;
    press(control(keys::sound1, false, 5));
    CHECK(edits == before);
    // No step held: not taken, nothing edited, for every button.
    for (u32 b = 0; b < 8; ++b) {
        const Control c = control(keys::sound1 + b, true, -1);
        CHECK(!taken(c));
        press(c);
    }
    CHECK(edits == before);
    // Other keys are not taken and do nothing, step held or not.
    CHECK(!taken(control(keys::m1, true, 5)));
    CHECK(!taken(control(keys::step1, true, 5)));
    press(control(keys::m1, true, 5)); press(control(keys::step1, true, 5));
    CHECK(edits == before);
    // Step 0 counts as held.
    CHECK(taken(control(keys::sound1, true, 0)));
    press(control(keys::sound2, true, 0));
    CHECK(edits == before + 1 && last_step == 0 && last_velocity == 8);
    // Aux tracks: still taken (the track must not switch), but no edit.
    const u32 mid = edits;
    for (u32 t = 8; t < 16; ++t) {
        const Control c = control(keys::sound4, true, 3, t);
        CHECK(taken(c));
        press(c);
    }
    CHECK(edits == mid);
    // Even a track button released after being taken on an aux track does nothing.
    press(control(keys::sound4, false, 3, 9));
    CHECK(edits == mid);

    // Policy: nearest button, exact first, a tie goes to the lower one.
    CHECK(step_velocity::nearest_button(-1) == -1 && step_velocity::nearest_button(0) == -1);
    for (u32 b = 0; b < 8; ++b) CHECK(step_velocity::nearest_button(want[b]) == static_cast<i32>(b));
    CHECK(step_velocity::nearest_button(1) == 0 && step_velocity::nearest_button(6) == 0);   // 4 | 8: 6 ties low
    CHECK(step_velocity::nearest_button(7) == 1);
    CHECK(step_velocity::nearest_button(12) == 1);                                            // 8 | 16 tie
    CHECK(step_velocity::nearest_button(13) == 2);
    CHECK(step_velocity::nearest_button(24) == 2 && step_velocity::nearest_button(25) == 3);  // 16 | 32
    CHECK(step_velocity::nearest_button(48) == 3 && step_velocity::nearest_button(49) == 4);  // 32 | 64
    CHECK(step_velocity::nearest_button(82) == 4 && step_velocity::nearest_button(83) == 5);  // 64 | 100
    CHECK(step_velocity::nearest_button(106) == 5 && step_velocity::nearest_button(107) == 6);// 100 | 112
    CHECK(step_velocity::nearest_button(119) == 6 && step_velocity::nearest_button(120) == 7);// 112 | 127
    CHECK(step_velocity::nearest_button(127) == 7);

    step_velocity::party = step_velocity::Party();   // the presses above were never released
    // Leds: no step held: not taken (stock's lights stay).
    CHECK(!lights(-1, 4).take);
    // Held: taken. Empty and unreadable: all eight off.
    TrackLights l = lights(5, -1);
    CHECK(l.take && all_off(l));
    // Exact value, the default 100 included: only its button, pink.
    for (u32 b = 0; b < 8; ++b) {
        l = lights(5, want[b]);
        CHECK(l.take && lit(l) == static_cast<i32>(b));
    }
    // Nearest, and a tie to the lower button.
    l = lights(5, 90);  CHECK(lit(l) == 5);           // 100
    l = lights(5, 70);  CHECK(lit(l) == 4);           // 64
    l = lights(5, 12);  CHECK(lit(l) == 1);           // tie 8 | 16: the lower
    l = lights(5, 82);  CHECK(lit(l) == 4);           // 64 | 100: 82 is nearer 64
    l = lights(5, 1);   CHECK(lit(l) == 0);
    // Aux track: taken, all off, whatever the velocity.
    l = lights(5, 8, 9);
    CHECK(l.take && all_off(l));
    // The pink is red + white.
    CHECK(step_velocity::pink == 0x7f80);

    // Pink party. An empty step (stored -1), track buttons 1 and 8 together: on, and again: off.
    stored = -1;
    step_velocity::party = step_velocity::Party();
    CHECK(!is_party() && !keys_lit().take);                      // off at start: nothing taken
    press(control(keys::sound1, true, 7)); CHECK(!is_party());   // only one button down
    press(control(keys::sound8, true, 7)); CHECK(is_party());    // the second completes it
    KeyLights k = keys_lit(); CHECK(k.take && all_pink(k));
    CHECK(!lights(7, 100).take);                                  // track lights not taken during the party
    release(keys::sound1, 7); release(keys::sound8, 7);
    CHECK(is_party() && step_velocity::party.down == 0);
    stored = -1;   // (lights() above set the velocity it was shown)
    press(control(keys::sound8, true, 7)); press(control(keys::sound1, true, 7));   // either order
    CHECK(!is_party() && !keys_lit().take && lights(7, 100).take);                  // off again: stock's / velocity lights back
    stored = -1;
    release(keys::sound1, 7); release(keys::sound8, 7);
    // Not toggled on a non-empty step.
    stored = 100;
    press(control(keys::sound1, true, 7)); press(control(keys::sound8, true, 7));
    CHECK(!is_party());
    release(keys::sound1, 7); release(keys::sound8, 7);
    stored = -1;
    // Not toggled with only one of the two buttons (or a third one instead), repeated presses included.
    press(control(keys::sound1, true, 7)); press(control(keys::sound1, true, 7));
    press(control(keys::sound3, true, 7)); CHECK(!is_party());
    release(keys::sound1, 7); release(keys::sound3, 7);
    press(control(keys::sound8, true, 7)); CHECK(!is_party());
    release(keys::sound8, 7);
    // Not toggled with no step held (the buttons are stock's then).
    press(control(keys::sound1, true, -1)); press(control(keys::sound8, true, -1)); CHECK(!is_party());
    // Releases clear the down state: 1 released before 8 is pressed, no toggle.
    release(keys::sound1, 7); release(keys::sound8, 7);
    CHECK(step_velocity::party.down == 0);
    press(control(keys::sound1, true, 7)); release(keys::sound1, 7);
    press(control(keys::sound8, true, 7)); CHECK(!is_party());
    release(keys::sound8, 7);
    // Buttons down before the step was held still count when the step is held for the second press.
    press(control(keys::sound1, true, -1));
    press(control(keys::sound8, true, 7)); CHECK(is_party());
    release(keys::sound1, 7); release(keys::sound8, 7);
    // Aux track: not toggled (instrument tracks only), stays as it is.
    press(control(keys::sound1, true, 7, 9)); press(control(keys::sound8, true, 7, 9));
    CHECK(is_party());
    release(keys::sound1, 7, 9); release(keys::sound8, 7, 9);
    press(control(keys::sound1, true, 7)); press(control(keys::sound8, true, 7)); CHECK(!is_party());   // back off
    release(keys::sound1, 7); release(keys::sound8, 7);
    // Key lights never take part in the edit: the velocity edit itself is unchanged.
    const u32 edited = edits;
    press(control(keys::sound4, true, 7)); CHECK(edits == edited + 1);
    return 0;
}
