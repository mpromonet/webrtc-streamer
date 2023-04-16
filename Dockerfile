# build
FROM ubuntu:22.04 as builder
LABEL maintainer=michel.promonet@free.fr
ARG USERNAME=dev
WORKDIR /webrtc-streamer
COPY . /webrtc-streamer

ENV PATH /depot_tools:$PATH

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates wget git python3 python3-pkg-resources g++ autoconf automake libtool xz-utils libpulse-dev libasound2-dev libgtk-3-dev libxtst-dev libssl-dev librtmp-dev cmake make pkg-config p7zip-full sudo \
	&& useradd -m -s /bin/bash $USERNAME \
	&& echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
	&& chmod 0440 /etc/sudoers.d/$USERNAME \
	&& git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git /depot_tools \
	&& mkdir /webrtc \
	&& cd /webrtc \
	&& fetch --no-history --nohooks webrtc \
	&& sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS \
	&& gclient sync \
	&& cd /webrtc-streamer \
	&& cmake . && make \
	&& cpack \
	&& mkdir /app && tar xvzf webrtc-streamer*.tar.gz --strip=1 -C /app/ && rm webrtc-streamer*.tar.gz \
	&& rm -rf /webrtc/src/out \
	&& apt-get clean && rm -rf /var/lib/apt/lists/ 

# run
FROM ubuntu:22.04

WORKDIR /app
COPY --from=builder /app/ /app/

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends libssl-dev libasound2 libgtk-3-0 libxtst6 libpulse0 librtmp1 avahi-utils \
	&& apt-get clean && rm -rf /var/lib/apt/lists/ \
	&& ./webrtc-streamer -V

ENTRYPOINT [ "./webrtc-streamer" ]
CMD [ "-C", "config.json" ]
