# Third-Party Notices

ntscreenshot is licensed under Apache-2.0. The following third-party components retain their own copyrights and licenses. This summary is provided for convenience; the upstream license text and the license metadata installed by vcpkg are authoritative.

| Component | Use | License |
| --- | --- | --- |
| Qt 6 | Application framework and deployment runtime | LGPL-3.0-only, GPL alternatives, or commercial license, depending on the Qt distribution |
| OpenCV | Image processing, detection, and QR/barcode support | Apache-2.0 |
| QHotkey | Cross-platform global hotkeys; source is under `src/libs/QHotkey` | BSD-3-Clause |
| Marked | Markdown rendering; bundled `marked.umd.js` | MIT |
| Haar cascade data | Bundled face-detection cascade derived from OpenCV/Intel data | License notice embedded in the XML file |
| Unicode-to-pinyin table | Local-search transliteration data under `src/core/pinyin` | Project-maintained legacy generated data, distributed under the project Apache-2.0 license |
| Karpathy guidelines skill | Optional bundled assistant guidance under `src/resource/skills/karpathy-guidelines` | MIT (declared in the bundled skill metadata) |
| libjpeg-turbo | OpenCV image codec dependency | BSD-3-Clause and IJG terms |
| libpng | OpenCV image codec dependency | libpng-2.0 |
| zlib | Compression dependency | Zlib |
| quirc | QR decoding dependency | ISC |

The Windows packaging script copies available vcpkg copyright files and QHotkey's license into the package's `licenses` directory. Qt deployment and redistribution must comply with the Qt license selected by the distributor. In particular, distributors using LGPL Qt builds must include the applicable Qt license texts, preserve the user's relinking and replacement rights, and meet the corresponding source-code requirements. See the [Qt licensing documentation](https://doc.qt.io/qt-6/licensing.html).

QHotkey's full license is available at `src/libs/QHotkey/LICENSE`. The Marked copyright header is retained in `src/resource/html/marked.umd.js`, and the cascade license is retained in `src/resource/opencv/haarcascade_frontalface_default.xml`.
