# build
FROM ubuntu:24.04 AS builder
LABEL maintainer=michel.promonet@free.fr
ARG USERNAME=dev
ARG USERID=10000
WORKDIR /build/webrtc-streamer

COPY . .

ENV PATH /depot_tools:/build/webrtc/src/third_party/llvm-build/Release+Asserts/bin:$PATH

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates wget git python3 python3-pkg-resources g++ autoconf automake libtool xz-utils libpulse-dev libasound2-dev libgtk-3-dev libxtst-dev libssl-dev librtmp-dev cmake make pkg-config p7zip-full sudo \
	&& groupadd --gid $USERID $USERNAME && useradd --uid $USERID --gid $USERNAME -m -s /bin/bash $USERNAME \
	&& echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
	&& chmod 0440 /etc/sudoers.d/$USERNAME \
	&& git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git /depot_tools \
	&& mkdir ../webrtc \
	&& cd ../webrtc \
	&& fetch --nohooks webrtc \
	&& cd ../webrtc-streamer \
	&& cmake . && make \
	&& make install \
	&& git clean -xfd \
	&& find ../webrtc/src -type d -name .git -exec rm -rf {} \; || true \
	&& rm -rf ../webrtc/src/out \
	&& apt-get clean && rm -rf /var/lib/apt/lists/ \
	&& chown -R $USERNAME:$USERNAME ../webrtc

USER $USERNAME

# run
FROM ubuntu:24.04
LABEL maintainer=michel.promonet@free.fr

WORKDIR /usr/local/share/webrtc-streamer

COPY --from=builder /usr/local/bin/webrtc-streamer /usr/local/bin/
COPY --from=builder /usr/local/share/webrtc-streamer/ /usr/local/share/webrtc-streamer/

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends libssl-dev libasound2-dev libgtk-3-0 libxtst6 libsm6 libpulse0 librtmp1 avahi-utils \
	&& useradd -m user -G video,audio \
	&& apt-get clean && rm -rf /var/lib/apt/lists/ \
	&& webrtc-streamer -V

USER user

ENTRYPOINT [ "webrtc-streamer" ]
CMD [ "-C", "config.json" ]
