op-xy 1.2.1 mods on [open-xy](https://github.com/etcetc000/open-xy).

- [`extensions/step_velocity`](extensions/README.md): hold a step + track 1–8 sets its velocity
- `patch_click.py`: softer metronome tick
- `*-dig-notes.md`: 1.2.1 addresses (metronome, step entry, key leds)

needs sdk branch [`sdk/step-velocity`](https://github.com/loftkid/open-xy/tree/sdk/step-velocity) (on top of etcetc000/open-xy#414).

```sh
python3 sdk.py build --machine <machine> --extension extensions/step_velocity --stock-os <os.ldr> --out <out>
python3 patch_click.py soft <out>/candidate <out-soft>/candidate
```
