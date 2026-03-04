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

# Ideas

Scroll wheel
- Do/undo
- arrow key navigation? Would use shift as a mod to do up/down and default to left/right?
- Swap tabs in browser/code
- Swap workspaces???
- Zoom in/out (already done via mouse scroll with ctrl home mod)
Combos
Sticky keys/layers?
Gaming mode.
Positional Hold-Tap?