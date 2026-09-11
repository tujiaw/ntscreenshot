# Changelog

All notable changes will be documented in this file. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- User-facing FAQ, bilingual README, GitHub issue templates, and a documentation index.
- HTTP service settings now warn that the feature requires Python 3 when no interpreter is found, and warn when the chosen port is one browsers refuse to open (such as 22) instead of leaving the page unexplained.

### Changed

- README now leads with download, features, and support instead of internal release-readiness notes.
- README now includes a social preview and direct latest-release download links.
- GitHub Releases now include bilingual download instructions and categorized change notes.

### Fixed

- The HTTP service now reports "running" only after the port actually accepts connections, and start failures (port taken, no permission, missing Python) surface as a dialog with the interpreter's own message instead of a status that flips a moment later and an unreadable traceback.

### Removed

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
