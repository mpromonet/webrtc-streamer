FROM heroku/heroku:18
MAINTAINER michel.promonet@free.fr

WORKDIR /webrtc-streamer
COPY . /webrtc-streamer

# Get tools for WebRTC
RUN git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git /webrtc/depot_tools
ENV PATH /webrtc/depot_tools:$PATH

# Build 
RUN apt-get update && apt-get install -y --no-install-recommends g++ autoconf automake libtool xz-utils libasound2-dev libgtk-3-dev cmake p7zip-full \
	&& cd /webrtc \
	&& fetch --no-history --nohooks webrtc \
	&& sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS \
	&& gclient sync \
	&& cd /webrtc-streamer \
	&& cmake . && make \
	&& rm -rf /webrtc && rm -f *.a && rm -f src/*.o \
	&& apt-get clean && rm -rf /var/lib/apt/lists/

# Make port 8000 available to the world outside this container
EXPOSE 8000

# Run when the container launches
ENTRYPOINT [ "./webrtc-streamer" ]
CMD [ "-a", "-C", "config.json", "screen://" ]
