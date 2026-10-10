# Changelog

All notable changes will be documented in this file. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Added a tray image browser with asynchronous thumbnails, directory watching, drag and drop, GIF playback, and basic crop/rotate/flip/resize editing. Edits support undo/redo and atomic PNG/JPEG export, with source conflict checks before overwriting.

## [0.1.4] - 2026-10-08

### Changed

- Moved AI chat, browser tools, chat assets and text selection into the independent [auto-browser](https://github.com/tujiaw/auto-browser) project. Removed their screenshot toolbar, tray, hotkey and Settings entries. Existing user settings and history remain available for import.
- Removed Qt WebEngine and WebChannel from screenshot builds and CI. Clipboard AI form filling continues to use its separate integration.
- Added `scripts/build-win.ps1 -Package` to build the portable ZIP and SHA256 checksum in one command. OpenCV packaging includes only runtime libraries imported by the executable.
- Removed obsolete migration assets and generated files. Installed minimal OpenCV dependencies can be retained separately from disposable build caches.

### Fixed

- Prevented parallel Windows test links from racing while vcpkg copied shared OpenCV runtime DLLs.
- Prevented auto-browser text selection from completing screenshot capture prematurely after releasing a dragged selection. Capture overlays expose a native exclusion marker; update auto-browser to receive the matching exclusion logic.
- Added regression coverage for selection release, continued resizing and drawing, and explicit clipboard completion.


## [0.1.3] - 2026-09-27

### Added

- Text-selection actions can now be explicitly saved, cancelled, or reset to their defaults from Settings, with confirmation before deletion or reset.

### Changed

- Global text selection now reads Windows UI Automation selections asynchronously and falls back to a clipboard-preserving copy path when needed, avoiding UI stalls and preserving rich clipboard contents.
- The text-selection toolbar now follows the selected text bounds, supports Escape and keyboard focus, truncates long action labels, and uses localized built-in action labels and prompts.
- Closing the assistant window now stops the active request and clears the transient chat session instead of restoring a closed conversation.

### Fixed

- Improved text-selection popup reliability across supported Windows applications and prevented stale selection reads from reopening the toolbar.

## [0.1.2] - 2026-09-26

### Added

- A visible browser panel beside the AI conversation with an address bar, back / forward / reload / home controls, and a built-in start page that introduces browser use. It keeps one off-the-record login session per conversation and exposes structured browser-use actions, handing control to you automatically when a page asks for a password or verification code. Manual interaction pauses automation, and AI resumes on its own once the page leaves the challenge; stopping is handled by the chat input's stop button. The chat window remembers the last Web / browser choice and the pane widths. Browser contract tests cover DOM actions, authentication handoff, cookie isolation, cancellation, and closure.

- A conversation Web toggle beside the model selector enables background WebEngine search and dynamic webpage reading, with per-request temporary profiles, cancellation, timeouts, and cleanup on conversation closure.

- User-facing FAQ, bilingual README, GitHub issue templates, and a documentation index.
- HTTP service settings now warn that the feature requires Python 3 when no interpreter is found, and warn when the chosen port is one browsers refuse to open (such as 22) instead of leaving the page unexplained.

### Changed

- Improved screenshot annotation controls, long-screenshot stitching, and conservative automatic border cropping.
- Limited the CI build to Windows.
- README now leads with download, features, and support instead of internal release-readiness notes.
- README now includes a social preview and direct latest-release download links.
- GitHub Releases now include bilingual download instructions and categorized change notes.

### Fixed

- The HTTP service now reports "running" only after the port actually accepts connections, and start failures (port taken, no permission, missing Python) surface as a dialog with the interpreter's own message instead of a status that flips a moment later and an unreadable traceback.

### Removed

- The assistant's local system tools — file read/write/edit, directory listing, Python execution, and shell/PowerShell commands — along with the Tavily search tool that was never wired into a conversation. The built-in WebEngine browser is now its only capability, so the model can search and read the web and nothing else. The "工具管理" and "网络搜索" settings pages and the "allow tool execution" confirmation prompt went with them.
- Internal agent planning docs that were not part of the public product surface.

## [0.1.1] - 2026-09-10

### Changed

- Refined the HTTP service settings layout, status presentation, and Windows build-tool discovery.

### Fixed

- Fixed clipped radio indicators and HTTP service configuration being compressed by multi-line addresses.
- Fixed `python http.server` argument compatibility across supported Python versions.

## [0.1.0] - 2026-08-27

### Added

- Initial public preview of the screenshot and desktop productivity application.
