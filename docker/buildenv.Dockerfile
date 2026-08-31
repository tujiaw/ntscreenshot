# 仅含编译依赖，不含 Qt；镜像层可被 Docker 缓存，避免每次拉 debian 再 apt
FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN set -eux; \
    apt-get update -qq; \
    apt-get install -y -qq --no-install-recommends \
      ca-certificates \
      ccache \
      cmake \
      ninja-build \
      build-essential \
      libgl1-mesa-dev \
      libglu1-mesa-dev \
      libopencv-dev \
      libx11-dev \
      libxcb-cursor0 \
      libxcb-xfixes0-dev \
      libxtst-dev \
      libxi-dev \
      libxkbfile-dev \
      libnss3 \
      libasound2 \
      libxkbcommon0 \
      libxcomposite1 \
      libxdamage1 \
      libxfixes3 \
      libxrandr2 \
      libgbm1 \
      libdrm2 \
      libcups2 \
    ; \
    rm -rf /var/lib/apt/lists/*

WORKDIR /work
