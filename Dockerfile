FROM heroku/cedar
LABEL maintainer michel.promonet@free.fr

WORKDIR /app

# Build WebRTC
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH /app/depot_tools:$PATH
RUN mkdir /webrtc
RUN cd /webrtc && fetch --no-history webrtc
RUN cd /webrtc/src && gn gen out/Release --args='is_debug=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false'
RUN cd /webrtc/src && ninja -C out/Release 

# Build live555
RUN wget http://www.live555.com/liveMedia/public/live555-latest.tar.gz -O - | tar xzf -
RUN cd live && ./genMakefiles linux
RUN cd live && make install PREFIX=/tmp


# Build webrtc-streamer
ADD . /app
RUN make PREFIX=/tmp

# Make port 8000 available to the world outside this container
EXPOSE 8000

# Run when the container launches
CMD "./webrtc-streamer*" "rtsp://217.17.220.110/axis-media/media.amp"
