# build
FROM ubuntu:18.04 as builder
LABEL maintainer=michel.promonet@free.fr

ARG ARCH=armv7l

WORKDIR /webrtc-streamer
COPY . /webrtc-streamer

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates wget git python xz-utils cmake make crossbuild-essential-armhf \
	&& mkdir /webrtc \
	&& git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git /webrtc/depot_tools \
	&& export PATH=/webrtc/depot_tools:$PATH \
	&& cd /webrtc \
	&& fetch --no-history --nohooks webrtc \
	&& sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS \
	&& /webrtc/src/build/linux/sysroot_scripts/install-sysroot.py --arch=arm \
	&& gclient sync \
	&& cd /webrtc-streamer \
	&& cmake -DCMAKE_SYSTEM_PROCESSOR=${ARCH} -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY -DWEBRTCDESKTOPCAPTURE=OFF . && make \
	&& cpack \
	&& mkdir /app && tar xvzf webrtc-streamer*.tar.gz --strip=1 -C /app/ \
	&& rm -rf /webrtc && rm -f *.a && rm -f src/*.o \
	&& apt-get clean && rm -rf /var/lib/apt/lists/

# run
FROM balenalib/raspberry-pi

WORKDIR /app
COPY --from=builder /app/ /app/

EXPOSE 8000

ENTRYPOINT [ "./webrtc-streamer" ]
CMD [ "-a", "-C", "config.json", "screen://" ]
