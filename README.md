# hypr-appmenu

Hyprland plugin providing the `org_kde_kwin_appmenu` Wayland protocol. It forwards application menu paths to the D-Bus AppMenu registrar (`com.canonical.AppMenu.Registrar`).

<div align="center">
  <img width="857" height="536" alt="showcase" src="https://github.com/user-attachments/assets/d32ca749-e5d4-42e3-9f52-3f43f5eacd25" />
</div>

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
