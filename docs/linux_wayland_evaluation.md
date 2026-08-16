# Linux Wayland Evaluation (Phase 39.2)

Status: EVALUATION COMPLETE — Decision: **Keep X11 with XWayland compatibility, do not add a native Wayland backend at this time.**

## Background

Aurora's Linux desktop backend (`aurora/src/std/gui.cpp`) is built directly on X11 (`Xlib`):
window creation, event loop, widget rendering, global hotkeys (`XGrabKey`), system tray
(`_NET_SYSTEM_TRAY_S0` XEmbed), drag & drop (`XDND`), clipboard (`XSetSelectionOwner`).

Most mainstream distributions run Wayland sessions by default now (Fedora, Ubuntu 21.10+,
Debian 12+), so Wayland compatibility is a real deployment concern.

## Options Considered

### Option 1: X11-only (keep current) — SELECTED
- Every X11 API Aurora uses works unchanged.
- XWayland provides a compatibility layer on Wayland sessions: X11 apps run unmodified,
  windowed. Compositor-side features (transparency, shadows) still apply through XWayland.
- **Caveats**: XWayland is a compatibility layer, not a compositor integration.
  - Global hotkeys via `XGrabKey` only fire while the app has focus under some compositors.
  - System tray XEmbed requires a tray host; GNOME (the most common Wayland DE) does not
    host XEmbed trays — the tray icon may not appear on stock GNOME.
  - DND works via XWayland but file drops into sandboxed apps (Flatpak/Snap) are restricted.
- Effort: 0 (no code).

### Option 2: Native Wayland backend (wlroots-style layer) — DEFERRED
- Would require a second rendering path in `gui.cpp`: `wl_display`, `wl_surface`,
  `xdg_shell` for windows, `wl_seat`/`wl_pointer`/`wl_keyboard` for input, `xdg_activation`,
  `org.freedesktop.Notifications` via DBus for notifications, `StatusNotifierItem` via DBus
  for tray, `gtk-layer-shell` for panels.
- 1200+ lines of new, platform-only code that duplicates the X11 path for no immediate
  functional gain on desktop CI.
- Portal-based global shortcuts (`org.freedesktop.portal.GlobalShortcuts`) and file access
  (`org.freedesktop.portal.FileChooser`) would be needed for parity with X11 features.
- Wayland lacks a universal system tray protocol; `StatusNotifierItem` (AppIndicator) is the
  closest standard and is supported by KDE Plasma, XFCE, Cinnamon, LXQt, and via
  AppIndicator extension on GNOME.

### Option 3: Abstraction layer (X11 + Wayland dual backend)
- Introduce a `gui_backend` vtable with `create_window`, `pump_events`, etc., and two
  implementations. Cleanest long term, but a large refactor of `gui.cpp` (~150 functions)
  with high regression risk.

## Recommendation

1. **Ship X11 + XWayland** as the supported desktop path for v2.x (already working).
2. **Revisit native Wayland support in a dedicated phase** (post-WASM) when the ecosystem
   demands: implement a `gui_wayland.cpp` backend using libwayland + xdg-shell, gated
   behind a compile flag (`AURORA_WAYLAND=ON`), auto-detected at runtime by checking
   `$XDG_SESSION_TYPE == "wayland"` with fallback to X11.
3. Tray on Wayland: prefer `StatusNotifierItem` over XEmbed; GNOME users install the
   AppIndicator extension. Document this in `docs/platform_guides.md`.

## Files
- `aurora/src/std/gui.cpp` — X11 backend (current, unchanged).
- `aurora/src/std/desktop.cpp` — Linux tray/hotkeys/DND/clipboard (current, unchanged).
- `aurora/src/std/gui_wayland.cpp` — **optional** native backend, NOT created yet.
