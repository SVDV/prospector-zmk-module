# Prospector ZMK Module

All the necessary stuff for [Prospector](https://github.com/carrefinho/prospector) to display things with ZMK. Currently functional albeit barebones.

## Features

Three views you switch between with a swipe:

- **Layer roller**: the highest active layer, held modifiers and caps word, a battery and link indicator per half, and the host (USB or Bluetooth profile)
- **Live map**: your keyboard drawn from its physical layout, with per-key heat for the current session, held keys, and how your presses split between the halves
- **WPM gauge**: typing speed on a dial, plus the active layer, modifiers and a battery gauge per half

Also:

- Display brightness control via keyboard shortcuts and touch swipes
- Original and Catppuccin Mocha colour themes

## Installation

Your ZMK keyboard should be set up with a dongle as central.

Add this module to your `config/west.yml` with these new entries under `remotes` and `projects`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: carrefinho                            # <--- add this
      url-base: https://github.com/carrefinho     # <--- and this
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: prospector-zmk-module                 # <--- and these
      remote: carrefinho                          # <---
      revision: main                              # <---
  self:
    path: config
```

Then add the `prospector_adapter` shield to the dongle in your `build.yaml`:

```yaml
---
include:
  - board: seeeduino_xiao_ble
    shield: [YOUR KEYBOARD SHIELD]_dongle prospector_adapter
```

For more information on ZMK Modules and building locally, see [the ZMK docs page on modules.](https://zmk.dev/docs/features/modules)

## Usage

For split keyboards, since the peripheral battery widget uses the order in which peripherals were paired to arrange the sub-widgets, after flashing the dongle, pair the left side first and then the right side. For more than two peripherals, pair them in a left to right order.

The layer roller shows layers' `display-name` property whenever available, and will fall back to the layer index otherwise. To add a `display-name` property to a keymap layer:

```dts
keymap {
  compatible = "zmk,keymap";
  base {
    display-name = "Base";           # <--- add this
    bindings = <
      ...
    >;
  }
}
```

### Live map

The live map draws the keys from your keyboard's `zmk,physical-layout`, which the dongle needs in its devicetree as the chosen physical layout. A chosen `zmk,matrix-transform` makes ZMK ignore physical layouts, so use this instead in the dongle overlay:

```dts
#include "my_keyboard_layouts.dtsi"

&my_keyboard_layout {
    transform = <&default_transform>; // the dongle's own matrix transform
    kscan = <&mock_kscan>;            // the dongle's mock kscan
};

/ {
    chosen {
        zmk,kscan = &mock_kscan;
        zmk,physical-layout = &my_keyboard_layout;
    };
};
```

Keys on the left half of the layout count towards the left share. A session starts over after `CONFIG_PROSPECTOR_LIVE_MAP_IDLE_RESET_MIN` minutes without a key press, and whenever the dongle restarts.

### Touch swipes

The touch panel reports swipes in its own portrait orientation, and the module maps them through the display rotation. By default, swiping left or right switches views and swiping up or down changes brightness. Set `CONFIG_PROSPECTOR_SWIPE_VIEWS_VERTICAL=y` to swap the axes.

### Display Brightness Control

The module includes a `&disp_bri` behavior for controlling display brightness from your keymap:

```dts
#include <dt-bindings/zmk/display_brightness.h>

// In behaviors section of keymap:
disp_bri: disp_bri {
    compatible = "zmk,behavior-display-brightness";
    #binding-cells = <2>;
};

// In keymap bindings:
&disp_bri DISP_BRI_INC 0  // Increase brightness
&disp_bri DISP_BRI_DEC 0  // Decrease brightness
&disp_bri DISP_BRI_TOG 0  // Toggle on/off
```

## Configuration

To customize, add config options to your `config/[YOUR KEYBOARD SHIELD].conf` like so:
```ini
CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR=n
CONFIG_PROSPECTOR_DEFAULT_BRIGHTNESS=80
```

### Available config options:
| Name                                              | Description                                                               | Default      |
| ------------------------------------------------- | --------------------------------------------------------------------------| ------------ |
| `CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR`      | Use ambient light sensor for auto brightness, set to `n` if building without one                              | y            |
| `CONFIG_PROSPECTOR_DEFAULT_BRIGHTNESS`            | Set default display brightness when not using ambient light sensor        | 50 (1-100)   |
| `CONFIG_PROSPECTOR_BRIGHTNESS_STEP`               | Brightness adjustment step size for keyboard controls                     | 10 (1-100)   |
| `CONFIG_PROSPECTOR_ROTATE_DISPLAY_180`            | Rotate the display 180 degrees                                            | n            |
| `CONFIG_PROSPECTOR_LAYER_ROLLER_ALL_CAPS`         | Convert layer names to all caps                                           | n            |
| `CONFIG_PROSPECTOR_THEME_CATPPUCCIN_MOCHA`        | Use the Catppuccin Mocha palette instead of the original black theme      | n            |
| `CONFIG_PROSPECTOR_SWIPE_VIEWS_VERTICAL`          | Swipe up/down to switch views and left/right for brightness               | n            |
| `CONFIG_PROSPECTOR_VIEW_LIVE_MAP`                 | Include the live map view                                                 | y            |
| `CONFIG_PROSPECTOR_LIVE_MAP_IDLE_RESET_MIN`       | Idle minutes before the live map starts a new session (0 = only on reboot) | 30           |
| `CONFIG_PROSPECTOR_VIEW_GAUGE`                    | Include the WPM gauge view (enables `CONFIG_ZMK_WPM`)                     | y            |
