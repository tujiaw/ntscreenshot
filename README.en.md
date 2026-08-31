# ntscreenshot

[简体中文](README.md)

A free, open-source Windows screenshot and desktop productivity app. Capture, pin, annotate, scrolling screenshots, clipboard history, and local search stay on your machine. AI and OCR are opt-in and off by default.

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-0078D4.svg?logo=windows)](docs/faq.en.md)
[![Qt](https://img.shields.io/badge/Qt-6-41CD52.svg?logo=qt)](docs/building.en.md)
[![Release](https://img.shields.io/github/v/release/tujiaw/ntscreenshot?include_prereleases)](https://github.com/tujiaw/ntscreenshot/releases)
[![GitHub stars](https://img.shields.io/github/stars/tujiaw/ntscreenshot)](https://github.com/tujiaw/ntscreenshot/stargazers)

**[Download the Windows portable build](https://github.com/tujiaw/ntscreenshot/releases)** · [FAQ](#faq) · [Build from source](docs/building.en.md)

Extract `ntscreenshot-x64-Release.zip` and run `ntscreenshot.exe`. No installer. If a release archive is not published yet, follow the [build guide](docs/building.en.md).

![Capture and annotation](ntscreenshot_demo.png)
![Pinning and productivity tools](ntscreenshot_demo2.png)

## Why ntscreenshot

The built-in Windows snipping tool is fine for a quick copy. It does not pin images, stitch long pages, or keep a local clipboard history. Commercial tools are polished, but they are often closed-source or subscription-based. ntscreenshot puts the daily workflow in one tray app, with public source and a default-offline design.

## Features

**Capture**
- Region capture, window snapping, pixel-level adjustment
- Magnifier and color picker (`C` copies the current color)
- Pen, arrow, rectangle, ellipse, text, mosaic, undo
- Scrolling capture and GIF recording
- QR / barcode recognition; optional OCR

**Pinning and productivity**
- Image pins with borders and multi-pin management
- Local clipboard history
- Text-selection toolbar
- Local file, folder, and browser-bookmark search

**Desktop**
- Tray app, global hotkeys, launch at sign-in
- Light and dark themes
- Optional LLM assistant, web search, and image hosting — enabled only in Settings

## 30-second start

1. Launch the app and find it in the system tray. Open Settings once to confirm hotkeys.
2. Press `F5` to capture: drag a region, annotate from the toolbar, then copy, save, pin, or start a scrolling capture.
3. Press `F6` to pin the clipboard image on the desktop.
4. Turn on OCR or AI in Settings only if you need them, using your own keys.

| Hotkey | Action |
| --- | --- |
| `F5` | Screenshot |
| `F6` | Pin |

Change hotkeys in tray → Settings if they clash with a game or another app.

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

## Privacy

Capture, pinning, clipboard history, and local search run offline by default. Text or images leave the machine only when you enable and invoke a network feature. See [Privacy](PRIVACY.en.md).

## Docs

- [FAQ](docs/faq.en.md): downloads, antivirus false positives, hotkeys, package size, OCR / AI
- [Build from source](docs/building.en.md)
- [Docs index](docs/README.md)
- [Project structure](docs/project-structure.md)
- [Changelog](CHANGELOG.md)

## Contributing

Bug reports, documentation fixes, and pull requests are welcome. Read [Contributing](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md). Report security issues privately as described in [Security](SECURITY.md). Do not file them as public issues.

## License

[Apache License 2.0](LICENSE). Copyright: [NOTICE](NOTICE). Third-party notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

If ntscreenshot helps you, a **Star** is the simplest way to support the project.
