ARG VARIANT=ubuntu-22.04
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}

ENV PATH="$PATH:/workspaces/depot_tools"

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates wget git python3 python3-pkg-resources g++ autoconf automake libtool xz-utils libpulse-dev libasound2-dev libgtk-3-dev libxtst-dev libssl-dev librtmp-dev cmake make pkg-config p7zip-full \
	&& git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git /workspaces/depot_tools
