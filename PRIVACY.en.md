# Privacy

[简体中文](PRIVACY.md)

Last updated: 2026-10-07

ntscreenshot is a local desktop application. The project does not operate a central account or telemetry service.

## Data stored locally

Depending on enabled features, the application may store:

- preferences and integration credentials in `config/base.ini` under Qt's application-local data directory
- diagnostic logs under that application's `logs` directory
- Windows crash dumps named `ntscreenshot_*.dmp` in the working directory after a crash
- clipboard history in `%LOCALAPPDATA%/ClipboardLite/history.db` on Windows
- local-search indexes and cached favicons under Qt's application-local data directory
- screenshots or recordings in locations you explicitly choose

Clipboard history and local-search indexes can contain sensitive personal information. Protect the operating-system account and delete these files before sharing a profile or diagnostic bundle.

## Network features

Capture, pinning, annotation, clipboard, and local search do not require an ntscreenshot service. Data is sent over the network only when you enable and invoke an integration, including:

- LLM or assistant requests, which may include entered text and selected images
- web search queries
- OCR uploads
- URL fetching
- GitHub image uploads

AI chat, the embedded browser and text selection moved to [auto-browser](https://github.com/tujiaw/auto-browser). Optional clipboard AI form filling remains and uses separate configuration.

## Credentials

API keys and tokens entered in Settings are written to the local INI file without application-managed encryption. They are not packaged into releases or committed to the repository. Use least-privilege, revocable tokens and rotate them if configuration or logs may have been exposed.

## Control and deletion

Disable an integration to stop future requests. After quitting ntscreenshot, remove configuration, indexes, clipboard history, logs, and crash dumps from the paths above. Deletion is irreversible; back up anything you need first.

## Changes

Material privacy changes will be documented in this file and the [changelog](CHANGELOG.md).
