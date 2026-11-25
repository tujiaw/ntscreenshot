# Amplifier UI Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan inline.

**Goal:** Match the amplifier to the drawing panel's Google-style surface and improve information readability.

**Architecture:** Keep AmplifierWidget and its existing interface. Use ThemeManager tokens for custom painting and font metrics for stable DPI-aware geometry.

**Tech Stack:** C++17, Qt Widgets, QPainter.

**Spec:** ../specs/2026-11-25-amplifier-ui-design.md

## Global Constraints

No new dependencies. Preserve focus/input transparency, color format, configurable crosshair and detection mode. Preserve unrelated changes.

## Review Focus

Check dark/light contrast, RGB and English text fit, DPI scaling, screen edge placement, and crisp preview/crosshair alignment. Use an offscreen preview harness rather than adding implementation-mirroring tests for this visual adjustment.

## Task 1: Restyle AmplifierWidget

- [x] Update Amplifier.h with initialized state and shared geometry helpers.
- [x] Update Amplifier.cpp with translucent rounded card, clipped preview, separate coordinate/color/mode rows and ALT keycap. Reuse existing translated mode strings.
- [x] Compile the changed component and render light/dark previews at multiple scales and formats; inspect the output.
- [x] Review the diff and run git diff --check. Provide a concise conventional commit message; leave changes uncommitted for review.

## Verification results

- Standalone Qt build compiled the actual Amplifier.cpp and its MOC successfully.
- Rendered and inspected theme/language previews. All 48 format/mode/theme/language/scale combinations passed color-value and transparent-corner assertions; edge placement passed at 100%, 150%, and 200% scale.
- Application sources compiled; default output linking was blocked by the running ntscreenshot.exe. Linked the same object files successfully as amplifier-verify.exe without stopping that app.
- The additional screenshot selection integration test did not pass (exit code 2 in the clean Qt runtime); it provided no diagnostic output. No claim is made that the complete regression suite passes.
- Scoped git diff --check passed. Existing whitespace in unrelated Settings.cpp was left unchanged.
- Changes remain uncommitted. Suggested message: style(capture): align amplifier UI with drawing panel
