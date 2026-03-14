# Future improvements

- Trackpad emitting trackpad instead of mouse events, support gestures
- Trackpad mouse layer active while ready pin is on the trackpad, not just while moving it
- nice! view display showing both battery %s

## Case edits

- Edit case/plate to cover the MCU with an OLED screen hole
- edit plates to cover the bottom of the encoder (it's ugly)
- ~edit plates to have cutout for power switch~
- edit case to allow for plastic sheet to slot above battery
- ~magnetic feet attachment~
- use non-magneric route with slotting feet into the base -- can slide out of the edge of the case
- slots for straps in the bottom?
- Color switches to not be red and super obvious (fine tip sharpie?)
- make wider so rubber can grab correctly when wrapped around pcb
- also make the PCB plateau further down to account for rubber
- ridge on the inside above the plateau? super small inset into wall
- add brace for trackpad

# things 2 get

- heat gun?
- mounting plastic tape/plate

# Trackpad

- gestures via different branch: https://github.com/petejohanson/zmk/tree/feat/pointers-move-scroll-ptp
- currently the mouse layer deactivates when i stop moving my finger on the touchpad
  - figure out a way to emit an event when ready starts as active and emit another when it deactivates, having the mouse layer active the whole time

# Ideas

Scroll wheel

- Do/undo
- arrow key navigation? Would use shift as a mod to do up/down and default to left/right?
- Swap tabs in browser/code
- Swap workspaces in hyprland/tabs in firefox based on layer with encoder?
- Zoom in/out (already done via mouse scroll with ctrl home mod)
- Start with larger scroll for mouse and adjust it by holding ctrl as a mod for it
- Fix expanded brace combos
- mouse layer when touchpad is active? so keys can be clicks?
- Arrow function combo?
- Add vim keystrokes?

Combos
Sticky keys/layers?
Gaming mode.
Positional Hold-Tap?
