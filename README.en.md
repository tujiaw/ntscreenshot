# ntscreenshot

[简体中文](README.md)

![ntscreenshot — open-source screenshot tool for Windows](docs/assets/social-preview.jpg)

**An open-source, privacy-first screenshot and pinning tool for Windows.** Capture, annotate, pin, scroll, and record GIFs in one workflow. Clipboard history and local search stay on your machine; AI and OCR are opt-in and off by default.

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-0078D4.svg?logo=windows)](docs/faq.en.md)
[![Qt](https://img.shields.io/badge/Qt-6-41CD52.svg?logo=qt)](docs/building.en.md)
[![Release](https://img.shields.io/github/v/release/tujiaw/ntscreenshot?include_prereleases)](https://github.com/tujiaw/ntscreenshot/releases)
[![GitHub stars](https://img.shields.io/github/stars/tujiaw/ntscreenshot)](https://github.com/tujiaw/ntscreenshot/stargazers)

**[Download the Windows portable build](https://github.com/tujiaw/ntscreenshot/releases/latest/download/ntscreenshot-x64-Release.zip)** · [What's new](https://github.com/tujiaw/ntscreenshot/releases/latest) · [FAQ](#faq) · [Build from source](docs/building.en.md)

Extract `ntscreenshot-x64-Release.zip` and run `ntscreenshot.exe`. No installer. If a release has no assets yet, follow the [build guide](docs/building.en.md).

## Features

### Capture and annotate

Press `F5` to select a region. Window snapping, pixel-level tweaks, a magnifier, and a color picker (`C` copies the color) stay in the overlay.

- Pen, line, arrow, rectangle, ellipse, text, mosaic, undo
- Copy, save, or pin; scrolling capture and GIF recording
- QR / barcode recognition; optional OCR and AI

![Region capture and annotation](screenshot.png)

### Local search

One box for local apps, files, folders, and browser bookmarks. Arrow keys select, Enter opens.

![Local search](local-search.png)

### Pinning and productivity

- `F6` pins a clipboard image on the desktop, with multi-pin and border options
- Local clipboard history
- A text-selection toolbar for quick actions on highlighted text

### Settings

Open Settings from the tray. Startup, light / dark theme, capture / pin / chat hotkeys, overlay opacity, and color format live here.

![Settings](settings.png)

### AI assistant (optional)

Off by default. Add your own endpoint in Settings, then open chat with the hotkey. No key means no network calls.

![AI chat](ai-chat.png)

## 30-second start

1. Launch the app, find it in the tray, and confirm hotkeys in Settings.
2. `F5` to capture: drag a region, annotate from the toolbar, then copy, save, or pin.
3. `F6` to pin.
4. Enable OCR or AI in Settings only if you need them, using your own keys.

| Hotkey | Action |
| --- | --- |
| `F5` | Screenshot |
| `F6` | Pin |

Change hotkeys if they clash with a game or another app.

## Why ntscreenshot

The built-in snipping tool is fine for a quick copy. It does not pin images, stitch long pages, or search local files. Commercial tools are polished, but they are often closed-source or subscription-based. ntscreenshot puts the daily workflow in one open-source tray app, and keeps data on your machine by default.

## Platforms

| Platform | Status |
| --- | --- |
| Windows 10 / 11 x64 | Supported |
| Linux | Not available |
| macOS | Not available |

<a id="faq"></a>

## FAQ

- Antivirus flags, dead hotkeys, no window after launch: [FAQ](docs/faq.en.md)
- OCR / AI / where data lives: [FAQ](docs/faq.en.md) · [Privacy](PRIVACY.en.md)

Capture, pinning, clipboard history, and local search run offline by default. Text or images leave the machine only when you enable and invoke a network feature. See [Privacy](PRIVACY.en.md).

## Docs

- [FAQ](docs/faq.en.md)
- [Build from source](docs/building.en.md)
- [Docs index](docs/README.md)
- [Changelog](CHANGELOG.md)

## Contributing

Bug reports, documentation fixes, and pull requests are welcome. Read [Contributing](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md). Report security issues privately as described in [Security](SECURITY.md).

[Apache License 2.0](LICENSE). Copyright: [NOTICE](NOTICE). Third-party notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

If ntscreenshot helps you, a **Star** is the simplest way to support the project.
