// Which velocity each track button sets, and which button shows a velocity.
// Pure functions, so tests drive them directly.
#pragma once
#include <opxy/extension.hpp>

namespace opxy { namespace extensions { namespace step_velocity {

// The velocity step component's values, one per track button 1-8.
const u32 table[8] = {4, 8, 16, 32, 64, 100, 112, 127};
// The lit button's led word: red in bits 8-14, white in the low byte. Pink
// is red + white; tune the tint here.
const u16 pink = 0x7f80;

inline bool is_track_button(u32 key) { return key >= keys::sound1 && key <= keys::sound8; }

// The velocity track button `key` sets.
inline u32 velocity_for(u32 key) { return table[key - keys::sound1]; }

// The button whose table value is nearest `velocity` (exact first; a tie goes
// to the lower one), or -1 for an empty or unreadable step (velocity < 1).
inline i32 nearest_button(i32 velocity) {
    if (velocity < 1) return -1;
    i32 best = 0, best_gap = 1 << 30;
    for (i32 i = 0; i < 8; ++i) {
        const i32 gap = velocity > static_cast<i32>(table[i]) ? velocity - static_cast<i32>(table[i])
                                                              : static_cast<i32>(table[i]) - velocity;
        if (gap < best_gap) { best = i; best_gap = gap; }
    }
    return best;
}

// The eight led words for a step held on `track` whose first note has
// `velocity`: all off for an empty step, an unreadable one or an
// aux track, otherwise only the nearest button, pink.
inline void light_words(u32 track, i32 velocity, u16* light) {
    for (u32 i = 0; i < 8; ++i) light[i] = 0;
    if (track >= 8) return;
    const i32 button = nearest_button(velocity);
    if (button >= 0) light[button] = pink;
}

// Pink party: which track buttons are down (bit n: button n+1), seen from the
// taken press and release events, since stock's held list never sees them, and
// whether the keyboard is all pink.
struct Party { u32 down; bool on; };

inline void track_button(Party& party, u32 key, bool pressed) {
    const u32 bit = 1u << (key - keys::sound1);
    party.down = pressed ? (party.down | bit) : (party.down & ~bit);
}

// The press of `key` (already recorded in `party.down`) completes the gesture: it is
// track button 1 or 8 and both are now down. The caller still checks that a step is
// held on an instrument track and is empty.
inline bool party_gesture(const Party& party, u32 key) {
    return (key == keys::sound1 || key == keys::sound8) && (party.down & 0x81) == 0x81;
}

// All 72 keyboard led words: pink.
inline void party_words(u16* light) { for (u32 i = 0; i < 72; ++i) light[i] = pink; }

}}}
