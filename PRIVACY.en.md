# Privacy

[简体中文](PRIVACY.md)

Last updated: 2026-09-12

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

The conversation's Web toggle is off by default. When enabled, the assistant can send search terms to Bing and load selected websites and their subresources through background WebEngine pages. Extracted content is returned to your configured model provider. No search API key is required. Each visit uses a temporary, off-the-record profile without persistent browser cookies or cache. Pages and profiles are released on completion, cancellation, timeout, or conversation closure. Tool results may remain in conversation history.

Those requests follow the selected provider's terms. Review the destination and avoid sending confidential content.

When the assistant and its tools are enabled, a model may request local file operations, URL access, Python, shell, or PowerShell using the current OS account. Use trusted providers, review tool calls, and run as a standard user.

## Credentials

API keys and tokens entered in Settings are written to the local INI file without application-managed encryption. They are not packaged into releases or committed to the repository. Use least-privilege, revocable tokens and rotate them if configuration or logs may have been exposed.

## Control and deletion

Disable an integration to stop future requests. After quitting ntscreenshot, remove configuration, indexes, clipboard history, logs, and crash dumps from the paths above. Deletion is irreversible; back up anything you need first.

## Changes

Material privacy changes will be documented in this file and the [changelog](CHANGELOG.md).
