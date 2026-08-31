# 在「与编译相同」的环境里打 Linux 便携包（linuxdeploy + Qt 插件）。
# 依赖已存在的编译镜像名（默认 ntscreenshot-buildenv），由 oneclick 脚本传入 --build-arg。
ARG BASE_IMAGE=ntscreenshot-buildenv
FROM ${BASE_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
    wget \
    ca-certificates \
    zstd \
    file \
    && rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    ARCH="$(uname -m)"; \
    case "$ARCH" in \
      x86_64) LD_ARCH=x86_64 ;; \
      aarch64) LD_ARCH=aarch64 ;; \
      *) echo "unsupported arch: $ARCH" >&2; exit 1 ;; \
    esac; \
    mkdir -p /opt/linuxdeploy; \
    wget -qO /opt/linuxdeploy/linuxdeploy.AppImage \
      "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${LD_ARCH}.AppImage"; \
    wget -qO /opt/linuxdeploy/linuxdeploy-plugin-qt.AppImage \
      "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${LD_ARCH}.AppImage"; \
    chmod +x /opt/linuxdeploy/linuxdeploy.AppImage /opt/linuxdeploy/linuxdeploy-plugin-qt.AppImage

# 容器内无 FUSE 时由 runtime 环境继承（oneclick 也会传）
ENV APPIMAGE_EXTRACT_AND_RUN=1
ENV LINUXDEPLOY=/opt/linuxdeploy/linuxdeploy.AppImage
ENV LINUXDEPLOY_PLUGIN_QT=/opt/linuxdeploy/linuxdeploy-plugin-qt.AppImage
