# build
FROM ubuntu:20.04 as builder
LABEL maintainer=michel.promonet@free.fr

WORKDIR /webrtc-streamer
COPY . /webrtc-streamer

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates wget git python g++ autoconf automake libtool xz-utils libpulse-dev libasound2-dev libgtk-3-dev libxtst-dev libssl-dev cmake make pkg-config p7zip-full \
        && git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git /webrtc/depot_tools \
        && export PATH=/webrtc/depot_tools:$PATH \
	&& cd /webrtc \
	&& fetch --no-history --nohooks webrtc \
	&& sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS \
	&& gclient sync \
	&& cd /webrtc-streamer \
	&& cmake . && make \
	&& cpack \
	&& mkdir /app && tar xvzf webrtc-streamer*.tar.gz --strip=1 -C /app/ \
	&& rm -rf /webrtc && rm -f *.a && rm -f src/*.o \
	&& apt-get clean && rm -rf /var/lib/apt/lists/

# run
FROM ubuntu:20.04

WORKDIR /app
COPY --from=builder /app/ /app/

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends libasound2 libgtk-3-0 libxtst6 libssl1.1 libpulse0 \
	&& apt-get clean && rm -rf /var/lib/apt/lists/ \
	&& ./webrtc-streamer -V

ENTRYPOINT [ "./webrtc-streamer" ]
CMD [ "-a", "-C", "config.json" ]
