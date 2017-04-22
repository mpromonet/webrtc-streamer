FROM heroku/cedar
LABEL maintainer michel.promonet@free.fr

WORKDIR /app

# Build WebRTC
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH /app/depot_tools:$PATH
RUN mkdir /webrtc
RUN cd /webrtc && fetch --no-history --nohooks webrtc
RUN cd /webrtc/src && git checkout master
RUN cd /webrtc && gclient sync
RUN cd /webrtc/src && gn gen out/Release --args='is_debug=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false use_gold=false'
RUN cd /webrtc/src && ninja -C out/Release

# Build webrtc-streamer
ADD . /app
RUN make 

# Make port 8000 available to the world outside this container
EXPOSE 8000

# Run when the container launches
CMD "./webrtc-streamer" "rtsp://217.17.220.110/axis-media/media.amp"
