MAKE SURE TO SET TRACKPAD PIN TO GPIO PIN UP:
azoteq,iqs5xx@74 {
    compatible = "azoteq,iqs5xx";
    reg = <0x74>;
    // ... SDA and SCL config ...
    rdy-gpios = <&gpio0 15 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>; /* Replace '15' with your chosen spare pin */
};

- trackpad
- bodge wiring of trackpad pins
    - Should be rewiring the P6 and P7 cols to go to P8 and P9 instead, moving the trackpad SDA and SCL to those two pins 
- don't need???

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

# things 2 get
- magnetic tape/strips for feet?
- heat shrink tubing and heat gun?
- keyboard foam?
- mounting plastic tape/plate

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

Combos
Sticky keys/layers?
Gaming mode.
Positional Hold-Tap?