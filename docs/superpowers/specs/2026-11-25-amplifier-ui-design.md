# Amplifier UI

Approved in chat on 2026-11-25.

Use the drawing panel's Google-style rounded surface, subtle shadow, theme colors and fine border. Keep a sharp pixel preview and configurable crosshair. Below it, separate coordinates, a color swatch with RGB/HEX text, and detection mode with an ALT keycap. Size the card from font metrics and DPI so English text and RGB values fit.

## Approaches

Selected: restyle the existing QWidget using ThemeManager tokens and QPainter, matching DrawPanel without adding dependencies. A color-only update would leave the crowded text layout; a larger tool panel would obstruct screenshot selection.

Keep capture, keyboard shortcuts, color formats and focus/input behavior. Follow both light and dark themes. Limit product changes to Amplifier.cpp and Amplifier.h; preserve unrelated working changes.

## Compact revision (2026-11-25)

User requested a more compact layout, no blank border around the preview, and a better background. The preview now reaches the card edges, with an 8 px corner radius and no outer shadow/inset. The information area uses 6 px padding, 2 px row gaps and a neutral white (#FFFFFF) / charcoal (#242424) background. Keep preview height at 88 px (before DPI scaling) and widen the sampled area for longer labels to preserve pixel magnification. Retain the existing three information rows and ALT keycap.

Verification: actual component compiled; all 48 preview combinations passed color, corner transparency and edge placement checks. Visually checked Chinese light and English dark at 100%, plus Chinese light at 200%. Scoped git diff --check passed. This revision did not rerun the broader screenshot integration test; its earlier exit code 2 remains unresolved.
