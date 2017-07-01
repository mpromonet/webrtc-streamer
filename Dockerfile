FROM heroku/cedar
LABEL maintainer michel.promonet@free.fr

WORKDIR /app
ADD . /app

# Get tools for WebRTC
RUN git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH /app/depot_tools:$PATH

# Build 
RUN mkdir /webrtc \
	&& cd /webrtc \
	&& fetch --no-history webrtc \
	&& cd src \
	&& gn gen out/Release --args='is_debug=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false enable_nacl=false rtc_enable_protobuf=false' \
	&& ninja -C out/Release \
	&& cd /app \
	&& make PREFIX=/tmp live555 \
	&& make PREFIX=/tmp all \
	&& rm -rf /webrtc

# Make port 8000 available to the world outside this container
EXPOSE 8000

# Run when the container launches
ENTRYPOINT [ "./webrtc-streamer" ]
CMD [ "rtsp://217.17.220.110/axis-media/media.amp", "rtsp://85.255.175.241/h264", "rtsp://85.255.175.244/h264", "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov" ]
