# build
FROM ubuntu:24.04 AS builder
LABEL maintainer=michel.promonet@free.fr
ARG USERNAME=dev
WORKDIR /build/webrtc-streamer

COPY . .

ENV PATH /depot_tools:/build/webrtc/src/third_party/llvm-build/Release+Asserts/bin:$PATH

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates wget git python3 python3-pkg-resources g++ autoconf automake libtool xz-utils libpulse-dev libasound2-dev libgtk-3-dev libxtst-dev libssl-dev librtmp-dev cmake make pkg-config p7zip-full sudo \
	&& useradd -m -s /bin/bash $USERNAME \
	&& echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
	&& chmod 0440 /etc/sudoers.d/$USERNAME \
	&& git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git /depot_tools \
	&& mkdir ../webrtc \
	&& cd ../webrtc \
	&& fetch --no-history --nohooks webrtc \
	&& sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS \
	&& gclient sync \
	&& cd ../webrtc-streamer \
	&& cmake -DCMAKE_INSTALL_PREFIX=/app -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ . && make \
	&& make install \
	&& rm -rf ../webrtc/src/out \
	&& apt-get clean && rm -rf /var/lib/apt/lists/ 

USER user

# run
FROM ubuntu:24.04
LABEL maintainer=michel.promonet@free.fr

WORKDIR /app/webrtc-streamer

COPY --from=builder /app/ /app/

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends libssl-dev libasound2-dev libgtk-3-0 libxtst6 libsm6 libpulse0 librtmp1 avahi-utils \
	&& useradd -m user -G video,audio \
	&& apt-get clean && rm -rf /var/lib/apt/lists/ \
	&& ./webrtc-streamer -V

USER user

ENTRYPOINT [ "./webrtc-streamer" ]
CMD [ "-C", "config.json" ]
