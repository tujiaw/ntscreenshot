# FAQ

[简体中文](faq.md)

## Where do I download it? How do I install it?

Open [GitHub Releases](https://github.com/tujiaw/ntscreenshot/releases), download `ntscreenshot-x64-Release.zip`, extract it, and run `ntscreenshot.exe`. There is no installer and no admin requirement.

If the release has no assets yet, only source is published — build it with the [build guide](building.en.md). Use binaries from this repository's Releases only.

## Why does antivirus flag the zip or exe?

The portable build is an unsigned Qt app. Windows Defender and other scanners often mark new unsigned binaries as unknown or potentially unwanted. That is usually a heuristic, not a confirmed infection.

Download only from this repository's Releases, submit the file to Microsoft for analysis if needed, and do not disable real-time protection as a workaround.

## F5 / F6 does nothing

Typical causes: the app is not running (check the tray), another screenshot tool or a game holds the hotkey, or an elevated window is in the foreground while ntscreenshot runs as a standard user.

Change the hotkeys in tray → Settings, or quit the other capture tool.

## I launched it and there is no window

ntscreenshot lives in the tray. Closing the main window does not quit. Right-click the tray icon to open the UI, Settings, or Exit. If a second launch returns immediately, an instance is already running.

## Why is the package large?

The release bundles Qt and WebEngine. That is heavier than a snipping-only utility, and it is expected with the current feature set. You can leave AI disabled; the runtime is still included in this build.

## OCR or QR codes do not work

QR / barcode detection is local (OpenCV) from the capture context menu. OCR must be enabled under **Settings → Image** with a configured service.

## Does AI upload my screenshots to your servers?

There is no ntscreenshot account or cloud. Traffic goes to the endpoint *you* configure, and only when you send a request. Keys are stored in a local INI file without extra encryption. Use revocable, least-privilege tokens. See [Privacy](../PRIVACY.en.md).

## Does it support macOS or Linux?

Windows 10 / 11 x64 only. macOS and Linux are not available.

## Where is my data? How do I wipe it?

Settings, logs, and search indexes live under Qt's application data directory. On Windows, clipboard history defaults to `%LOCALAPPDATA%/ClipboardLite/history.db`. Quit the app before deleting those files. See [Privacy](../PRIVACY.en.md).

## How do I file a useful issue?

Search existing issues first. Include OS, ntscreenshot version or commit, whether it reproduces, and redacted logs. Never paste API keys, chat contents, or clipboard data. Security reports follow [Security](../SECURITY.md).
