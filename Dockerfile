FROM heroku/heroku:16
MAINTAINER michel.promonet@free.fr

WORKDIR /webrtc-streamer
COPY . /webrtc-streamer

# Get tools for WebRTC
RUN git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH /webrtc-streamer/depot_tools:$PATH


# Build 
RUN apt-get update && apt-get install -y --no-install-recommends g++ autoconf automake libtool xz-utils libasound-dev \
        && mkdir /webrtc \
	&& cd /webrtc \
	&& fetch --no-history --nohooks webrtc \
	&& sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS \
	&& gclient sync \
	&& cd src \
	&& gn gen out/Release --args='is_debug=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false rtc_enable_protobuf=false use_custom_libcxx=false use_ozone=true rtc_include_pulse_audio=false rtc_build_examples=false' \
	&& make -C /webrtc-streamer live555 \
	&& ninja -C out/Release jsoncpp rtc_json webrtc \
	&& cd /webrtc-streamer \
	&& make all \
	&& rm -rf /webrtc \
	&& apt-get clean && rm -rf /var/lib/apt/lists/

# Make port 8000 available to the world outside this container
EXPOSE 8000

# Run when the container launches
ENTRYPOINT [ "./webrtc-streamer" ]
CMD [ "-a", "rtsp://217.17.220.110/axis-media/media.amp", "rtsp://85.255.175.241/h264", "rtsp://85.255.175.244/h264", "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov", "rtsp://b1.dnsdojo.com:1935/live/sys3.stream", "rtsp://streaming3.webcam.nl:80/n224/n224.stream", "rtsp://streaming3.webcam.nl:80/varik/varik.stream" ]
