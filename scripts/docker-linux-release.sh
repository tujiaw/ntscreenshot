#!/usr/bin/env bash
# Docker 内编译并打 Linux 绿色包（AppDir + tar.zst）。本仓库仅此一个 Linux 出包入口。
#
# 用法：./scripts/docker-linux-release.sh
#
# 环境变量：
#   QT_HOME              Qt 目录，默认 ~/Qt/6.8.3/gcc_64
#   OUT_DIR              构建与产物目录，默认 <项目>/build-docker
#   BUILD_ENV_IMAGE      编译依赖镜像，默认 ntscreenshot-buildenv
#   PACKAGE_IMAGE        打包镜像，默认 ntscreenshot-packageenv
#   CCACHE_DIR           ccache 目录，默认 <项目>/.cache/ccache
#   NT_FORCE_CONFIGURE   设为 1 时强制 CMake 重新配置
#   NT_REUSE_CONTAINER   设为 1 时使用持久编译容器（见 NT_BUILD_CONTAINER）
#   NT_BUILD_CONTAINER   持久容器名，默认 ntscreenshot-build-persist
#   SKIP_DOCKER_BUILD    设为 1 时跳过两个 docker build（镜像已存在时）
#   SKIP_COMPILE         设为 1 时跳过编译，仅打包（需已有 OUT_DIR/ntscreenshot）
#   SKIP_TARBALL         设为 1 时不生成 tar.zst
#   DOCKER_BUILD_OPTS    传给 docker build 的额外参数
#
set -euo pipefail

if [[ "${NT_INNER_PACKAGE:-0}" == "1" ]]; then
  export APPIMAGE_EXTRACT_AND_RUN="${APPIMAGE_EXTRACT_AND_RUN:-1}"
  ROOT="${ROOT:-/src}"
  BINARY="${BINARY:-/out/ntscreenshot}"
  QT_HOME="${QT_HOME:-/qt}"
  DIST="${DIST:-/out/linux-portable-bundle}"
  DESKTOP="${DESKTOP:-$ROOT/packaging/linux/ntscreenshot.desktop}"
  ICON="${ICON:-$ROOT/packaging/linux/ntscreenshot.png}"
  SKIP_TARBALL="${SKIP_TARBALL:-0}"
  LINUXDEPLOY="${LINUXDEPLOY:-/opt/linuxdeploy/linuxdeploy.AppImage}"
  LINUXDEPLOY_PLUGIN_QT="${LINUXDEPLOY_PLUGIN_QT:-/opt/linuxdeploy/linuxdeploy-plugin-qt.AppImage}"

  if [[ ! -f "$BINARY" || ! -x "$BINARY" ]]; then
    echo "未找到可执行文件: $BINARY" >&2
    exit 1
  fi
  if [[ ! -f "$QT_HOME/bin/qmake" ]]; then
    echo "未找到 Qt: $QT_HOME/bin/qmake" >&2
    exit 1
  fi
  if [[ ! -f "$DESKTOP" ]]; then
    echo "缺少 desktop: $DESKTOP" >&2
    exit 1
  fi
  if [[ ! -f "$ICON" ]]; then
    echo "缺少图标（与 desktop 中 Icon=ntscreenshot 对应）: $ICON" >&2
    exit 1
  fi
  if [[ ! -f "$LINUXDEPLOY" || ! -f "$LINUXDEPLOY_PLUGIN_QT" ]]; then
    echo "未找到 linuxdeploy（镜像应预装于 /opt/linuxdeploy）" >&2
    exit 1
  fi

  MACHINE="$(uname -m)"
  APPDIR="$DIST/AppDir"
  LINKDIR="$DIST/.linuxdeploy-linkdir"
  rm -rf "$DIST"
  mkdir -p "$DIST" "$LINKDIR"

  LD_CANON="linuxdeploy-${MACHINE}.AppImage"
  QT_CANON="linuxdeploy-plugin-qt-${MACHINE}.AppImage"
  ln -sf "$(realpath "$LINUXDEPLOY")" "$LINKDIR/$LD_CANON"
  ln -sf "$(realpath "$LINUXDEPLOY_PLUGIN_QT")" "$LINKDIR/$QT_CANON"
  LINUXDEPLOY_CANON="$LINKDIR/$LD_CANON"
  chmod +x "$LINUXDEPLOY_CANON" "$LINKDIR/$QT_CANON" 2>/dev/null || true

  echo "检查 ldd ..."
  if ldd "$BINARY" 2>/dev/null | grep -q 'not found'; then
    echo "错误: 存在未解析依赖" >&2
    ldd "$BINARY" 2>/dev/null | grep 'not found' || true
    exit 1
  fi

  export PATH="$QT_HOME/bin:$PATH"
  export LD_LIBRARY_PATH="$QT_HOME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

  echo "linuxdeploy -> $APPDIR"
  "$LINUXDEPLOY_CANON" --appdir "$APPDIR" \
    --executable "$BINARY" \
    --desktop-file "$DESKTOP" \
    --icon-file "$ICON" \
    --plugin qt

  if [[ ! -f "$APPDIR/AppRun" ]]; then
    echo "错误: 未生成 AppRun" >&2
    exit 1
  fi
  chmod -R a+rX "$APPDIR"

  DOC_DIR="$APPDIR/usr/share/doc/ntscreenshot"
  mkdir -p "$DOC_DIR/licenses"
  for document in LICENSE PRIVACY.md THIRD_PARTY_NOTICES.md; do
    if [[ ! -f "$ROOT/$document" ]]; then
      echo "缺少发布文档: $ROOT/$document" >&2
      exit 1
    fi
    cp "$ROOT/$document" "$DOC_DIR/"
  done
  cp "$ROOT/src/libs/QHotkey/LICENSE" "$DOC_DIR/licenses/QHotkey-BSD-3-Clause.txt"

  if [[ "$SKIP_TARBALL" != "1" ]]; then
    ARCHIVE="$DIST/ntscreenshot-linux-portable-${MACHINE}.tar.zst"
    ( cd "$DIST" && tar -caf "$ARCHIVE" AppDir )
    echo "归档: $ARCHIVE"
  fi
  echo "AppDir: $APPDIR/AppRun"
  exit 0
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QT_HOME="${QT_HOME:-$HOME/Qt/6.8.3/gcc_64}"
OUT_DIR="${OUT_DIR:-$ROOT/build-docker}"
BUILD_ENV_IMAGE="${BUILD_ENV_IMAGE:-ntscreenshot-buildenv}"
PACKAGE_IMAGE="${PACKAGE_IMAGE:-ntscreenshot-packageenv}"
CONTAINER_NAME="${NT_BUILD_CONTAINER:-ntscreenshot-build-persist}"
CCACHE_DIR="${CCACHE_DIR:-$ROOT/.cache/ccache}"
SKIP_DOCKER_BUILD="${SKIP_DOCKER_BUILD:-0}"
SKIP_COMPILE="${SKIP_COMPILE:-0}"

if [[ ! -f "$QT_HOME/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  echo "未找到 Qt6，请设置 QT_HOME" >&2
  exit 1
fi

mkdir -p "$OUT_DIR" "$CCACHE_DIR"

if [[ "$SKIP_DOCKER_BUILD" != "1" ]]; then
  echo "[1/4] 镜像 $BUILD_ENV_IMAGE ..."
  docker build ${DOCKER_BUILD_OPTS:-} \
    -f "$ROOT/docker/buildenv.Dockerfile" \
    -t "$BUILD_ENV_IMAGE" \
    "$ROOT/docker"

  echo "[2/4] 镜像 $PACKAGE_IMAGE ..."
  docker build ${DOCKER_BUILD_OPTS:-} \
    -f "$ROOT/docker/package.Dockerfile" \
    --build-arg "BASE_IMAGE=$BUILD_ENV_IMAGE" \
    -t "$PACKAGE_IMAGE" \
    "$ROOT/docker"
else
  echo "[1-2/4] 跳过 docker build（SKIP_DOCKER_BUILD=1）"
fi

run_cmake_build() {
  local configure_needed=0
  local cmake_args=(
    -S /src
    -B /out
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_PREFIX_PATH=/qt
  )

  if command -v ccache >/dev/null 2>&1; then
    export CCACHE_DIR=/ccache
    cmake_args+=(
      -DCMAKE_C_COMPILER_LAUNCHER=ccache
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    )
  fi

  if [[ ! -f /out/CMakeCache.txt || ! -f /out/build.ninja ]]; then
    configure_needed=1
  fi
  if [[ "${NT_FORCE_CONFIGURE:-0}" == "1" ]]; then
    configure_needed=1
  fi

  if [[ "$configure_needed" == "1" ]]; then
    echo "配置 CMake..."
    cmake "${cmake_args[@]}"
  else
    echo "复用 /out CMake 配置，增量编译..."
  fi

  cmake --build /out -j"$(nproc)"
  QT_QPA_PLATFORM=offscreen ctest --test-dir /out --output-on-failure
  if command -v ccache >/dev/null 2>&1; then
    ccache --show-stats || true
  fi
  ls -la /out/ntscreenshot
}

if [[ "$SKIP_COMPILE" != "1" ]]; then
  echo "[3/4] 编译 ..."
  if [[ "${NT_REUSE_CONTAINER:-0}" == "1" ]]; then
    if ! docker ps -a --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
      docker run -d --name "$CONTAINER_NAME" \
        -v "$ROOT:/src:ro" \
        -v "$QT_HOME:/qt:ro" \
        -v "$OUT_DIR:/out" \
        -v "$CCACHE_DIR:/ccache" \
        "$BUILD_ENV_IMAGE" \
        sleep infinity
    else
      docker start "$CONTAINER_NAME" >/dev/null
    fi
    docker exec "$CONTAINER_NAME" bash -c "$(declare -f run_cmake_build); run_cmake_build"
  else
    docker run --rm \
      -v "$ROOT:/src:ro" \
      -v "$QT_HOME:/qt:ro" \
      -v "$OUT_DIR:/out" \
      -v "$CCACHE_DIR:/ccache" \
      "$BUILD_ENV_IMAGE" \
      bash -c "$(declare -f run_cmake_build); run_cmake_build"
  fi
else
  echo "[3/4] 跳过编译（SKIP_COMPILE=1）"
  if [[ ! -x "$OUT_DIR/ntscreenshot" ]]; then
    echo "错误: 缺少 $OUT_DIR/ntscreenshot" >&2
    exit 1
  fi
fi

echo "[4/4] 绿色包 ..."
# libqtposition_nmea.so 依赖 Qt6SerialPort；截图应用不需要 NMEA。将 plugins 拷到 OUT_DIR 并去掉该插件后挂载覆盖 /qt/plugins。
docker run --rm \
  -v "$QT_HOME:/qt:ro" \
  -v "$OUT_DIR:/out" \
  "$BUILD_ENV_IMAGE" \
  bash -c 'set -euo pipefail; rm -rf /out/.qt-plugins-trim; mkdir -p /out/.qt-plugins-trim; cp -a /qt/plugins/. /out/.qt-plugins-trim/; rm -f /out/.qt-plugins-trim/position/libqtposition_nmea.so'

docker run --rm \
  -e APPIMAGE_EXTRACT_AND_RUN=1 \
  -e NT_INNER_PACKAGE=1 \
  -e SKIP_TARBALL="${SKIP_TARBALL:-0}" \
  -v "$ROOT:/src:ro" \
  -v "$OUT_DIR:/out" \
  -v "$QT_HOME:/qt:ro" \
  -v "$OUT_DIR/.qt-plugins-trim:/qt/plugins:ro" \
  "$PACKAGE_IMAGE" \
  bash /src/scripts/docker-linux-release.sh

echo "完成: $OUT_DIR/ntscreenshot"
echo "绿色包: $OUT_DIR/linux-portable-bundle/AppDir （AppRun）"
echo "归档:   $OUT_DIR/linux-portable-bundle/ntscreenshot-linux-portable-*.tar.zst"
