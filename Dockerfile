# build
FROM heroku/heroku:18 as builder
MAINTAINER michel.promonet@free.fr

WORKDIR /webrtc-streamer
COPY . /webrtc-streamer

RUN apt-get update && apt-get install -y --no-install-recommends g++ autoconf automake libtool xz-utils libasound2-dev libgtk-3-dev cmake p7zip-full \
        && git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git /webrtc/depot_tools \
        && export PATH=/webrtc/depot_tools:$PATH \
	&& cd /webrtc \
	&& fetch --no-history --nohooks webrtc \
	&& sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS \
	&& gclient sync \
	&& cd /webrtc-streamer \
	&& cmake . && make \
	&& cpack \
	&& rm -rf /webrtc && rm -f *.a && rm -f src/*.o \
	&& apt-get clean && rm -rf /var/lib/apt/lists/

# run
FROM heroku/heroku:18

WORKDIR /app
COPY --from=builder /webrtc-streamer/*.tar.gz /app/

RUN tar xvzf *.tar.gz

EXPOSE 8000

ENTRYPOINT [ "./webrtc-streamer" ]
CMD [ "-a", "-C", "config.json", "screen://" ]
