# hypr-appmenu

Hyprland plugin providing the `org_kde_kwin_appmenu` Wayland protocol. It forwards application menu paths to the D-Bus AppMenu registrar (`com.canonical.AppMenu.Registrar`).

## Installation

### Using hyprpm (recommended)

```sh
hyprpm add https://github.com/memtable/hypr-appmenu
hyprpm enable hypr-appmenu
```

### Manual build

```sh
make all
hyprctl plugin load $(pwd)/hypr-appmenu.so
```

## Verification

To verify that menus are registering:

```sh
# Watch D-Bus registration calls
busctl --user monitor com.canonical.AppMenu.Registrar
```

## Attribution

Based on the initial protocol skeleton by [Alex Hulbert](https://github.com/alexhulbert/HyprAppMenu).
