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

When Web is enabled, the assistant can send search terms to Google (falling back to Bing) and load selected websites and their subresources through background WebEngine pages. Extracted content is returned to your configured model provider. No search API key is required. Each background visit uses a temporary, off-the-record profile. Pages and profiles are released on completion, cancellation, timeout, or conversation closure. Tool results may remain in conversation history.

`browser_use` operates the visible browser on the right. It retains an off-the-record login session for the conversation, destroys it on conversation closure, and shares no cookies with background readers or other conversations. With Web enabled, AI can read pages, click controls, and fill ordinary forms. Authenticated page content may be sent to your model provider and retained in conversation history. Detected password or verification challenges pause automation and request human takeover. You can also pause AI by interacting directly with the page. While paused, AI performs no page reading and no automated actions, and resumes once the page leaves the challenge. Detection does not cover every website. Enter credentials directly into the website or local authentication dialog, not the AI conversation. Some websites prohibit embedded-browser authentication; external-browser sessions are not automatically transferred back.

Those requests follow the selected provider's terms. Review the destination and avoid sending confidential content.

Assistant tools can access websites. Local file operations, Python, shell, and PowerShell tools are not currently provided. Use trusted model providers and review website actions involving submission, sending, or account changes.

## Credentials

API keys and tokens entered in Settings are written to the local INI file without application-managed encryption. They are not packaged into releases or committed to the repository. Use least-privilege, revocable tokens and rotate them if configuration or logs may have been exposed.

## Control and deletion

Disable an integration to stop future requests. After quitting ntscreenshot, remove configuration, indexes, clipboard history, logs, and crash dumps from the paths above. Deletion is irreversible; back up anything you need first.

## Changes

Material privacy changes will be documented in this file and the [changelog](CHANGELOG.md).
