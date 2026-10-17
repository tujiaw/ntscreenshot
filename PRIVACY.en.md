# Privacy

[简体中文](PRIVACY.md)

Last updated: 2026-10-17

ntscreenshot is an offline-first desktop application without telemetry. Optional favorites sync uses GitHub authorization with a shared hosted service.

## Data stored locally

Depending on enabled features, the application may store:

- preferences and integration credentials in `config/base.ini` under Qt's application-local data directory
- diagnostic logs under that application's `logs` directory
- Windows crash dumps named `ntscreenshot_*.dmp` in the working directory after a crash
- clipboard history in `%LOCALAPPDATA%/ClipboardLite/history.db` on Windows
- favorites sync state in the same clipboard database and image caches under `favorite-sync-images` in Qt's application-local data directory
- favorites sync session credentials in Windows Credential Manager
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
- opt-in favorites cloud sync: GitHub authorization and favorite text, images, and sync metadata sent to the shared sync service

AI chat, the embedded browser and text selection moved to [auto-browser](https://github.com/tujiaw/auto-browser). Optional clipboard AI form filling remains and uses separate configuration.

Favorites cloud sync is off by default. The sign-in screen explains that signing in merges local and cloud favorites.
Ordinary clipboard history is not uploaded. GitHub identifies the account; repository access is not requested.
Favorites are stored in the shared Vercel service's Neon database and private Blob store, isolated by account. Its operator can read the content;
end-to-end encryption is not provided. Signing out keeps local and cloud favorites.
Deleting or unfavoriting an item syncs a tombstone; historical events and images remain on the server for offline recovery.
Contact the service operator to permanently erase cloud data. See the [deployment guide](cloud/clipboard-sync/README.md).

## Credentials

API keys and tokens entered in Settings are written to the local INI file without application-managed encryption. They are not packaged into releases or committed to the repository. Use least-privilege, revocable tokens and rotate them if configuration or logs may have been exposed.

Favorites sync session tokens are stored separately in Windows Credential Manager, not in the INI or clipboard database.
GitHub access tokens are used during the server-side sign-in exchange and are not persisted.

## Control and deletion

Disable an integration to stop future requests. After quitting ntscreenshot, remove configuration, indexes, clipboard history, logs, and crash dumps from the paths above. Deletion is irreversible; back up anything you need first.

## Changes

Material privacy changes will be documented in this file and the [changelog](CHANGELOG.md).
