# Changelog

All notable changes will be documented in this file. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- A visible browser panel beside the AI conversation with an address bar, back / forward / reload / home controls, and a built-in start page that introduces browser use. It keeps one off-the-record login session per conversation and exposes structured browser-use actions, handing control to you automatically when a page asks for a password or verification code. Manual interaction pauses automation, and AI resumes on its own once the page leaves the challenge; stopping is handled by the chat input's stop button. The chat window remembers the last Web / browser choice and the pane widths. Browser contract tests cover DOM actions, authentication handoff, cookie isolation, cancellation, and closure.

- A conversation Web toggle beside the model selector enables background WebEngine search and dynamic webpage reading, with per-request temporary profiles, cancellation, timeouts, and cleanup on conversation closure.

- User-facing FAQ, bilingual README, GitHub issue templates, and a documentation index.
- HTTP service settings now warn that the feature requires Python 3 when no interpreter is found, and warn when the chosen port is one browsers refuse to open (such as 22) instead of leaving the page unexplained.

### Changed

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
