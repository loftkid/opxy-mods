# OP-XY 1.2.1 metronome click: findings (read-only RE)

verdict: SYNTHESIZED (square-wave burst through a one-pole lowpass, linear decay). no PCM sample.

## chain
1. trigger (GUI/sequencer role): 8d5aaaa4 (transport tick path, gated by B[0x860c9b39]/+1).
   n = (transport[+8]-1)/240 ; skips if n == last n ([0x840a9eac]);
   click only if n % click_units[ts]==0 ; accent = (n % bar_units[ts]==0)
   tables (words): 0x840a9f40 click_units = 4,4,4,2,2,2 ; 0x840a9f58 bar_units = 12,16,20,12,14,24
   (ts index = low nibble of project byte +0x1f via 8d584418, min 5)
   -> 8d4dcb44(accent): bridge send, msg id 0xb2, 1 payload byte (accent) via 0x11a00ec0 on bridge 0x841460b4
2. receive (audio role): dispatcher 8d5b1260 region, jump table 0x840ab554 (ids 0xa1..0xc5), id 0xb2 -> 8d5b1aac
   -> 8d504fc4(accent): B[0x840479f0]=1 (trigger) ; word[0x08039108] = (accent==0)
3. render: L1 11a02688 = abs CALL 0x8d505148 (bytes 00de 508d 4851 0000), file off 0x5d18.
   8d505148 -> 8d504ffc(trigger) ; clears B[0x840479f0] after.
   trigger path 8d5050c8: inc = word[0x08039108]==0 ? 0x091a244a (1567.98 Hz G6, accent) : 0x066fa8b7 (1108.73 Hz C#6) @44.1k
     -> [0x08039118]
     level = B[0x84c69836]<<7<<16 (Q31) ; t = 0.2L+0.8L^2 ; A0 = t*0.15*0.5 -> [0x0803910c]
     decay step D = A0*0x002526e5 (=0.0011338 -> amplitude hits 0 in 882 samples = 20.0 ms) -> [0x08039110]
   generate path 8d505008 (64 samples per block, LSETUP):
     s = (phase bit31 ? +A : -A) ; y = 0.85*y + s (0x6cccccbf, corner ~1.14 kHz) ; A = max(A-D,0) ; phase += inc
     state: phase [0x08039114], filter y [0x0803911c]. NOT reset on trigger.
     output ADDED into 0x11b01148 (L) / 0x11b01248 (R), the L1 stereo out buffers
     (filled just before by 11a0b3f4 copy of 0x11b00f48/0x11b01048 = the mixed output)
   so: click is after the mix/master path, not in the track mixer, not in master EQ.
4. level byte 0x84c69836 = audio state 0x84c696fc + 0x13a (set by handler 8d5b1434, +0x139 next to it).

## patch sites (file offsets in os.ldr)
 0x13c32e imm 0x091a244a (accent inc)      0x13c336 imm 0x066fa8b7 (normal inc)
 0x13c2b4 imm 0x6cccccbf (lp coef)         0x13c386 imm 0x002526e5 (decay)
 0x13c34e 0x19999999 / 0x13c36e 0x13333340 (level taper / gain)
 0x5d18 L1 call to 8d505148 (hook point for a replacement renderer)
(imm = 32-bit at +2 of each 8-byte 'da0x hi lo 0000' word: bytes 'XX da lo16 hi16'... little endian hi,lo word order: da01 <hi16> <lo16>)

## unverified
- USB 'mtronome' source routing (list in 84058370, strings) not traced; 8d505148 has one call site only.
- exact meaning of 240-unit (likely 16th-note in 960/quarter) not confirmed.
- default level byte value not traced.
