import setupWebRTCProxy from '@rainforestqa/webrtc-client';

(function init() {
    if (!location.search.slice(1)) {
        if (typeof URLSearchParams != 'undefined') {
            alert("WebRTC stream name to connect is missing\n\nUsage :" + window.location + "?video=<WebRTC video stream name>&audio=<WebRTC audio stream name>&options=<WebRTC options>")
        } else {
            alert("WebRTC stream name to connect is missing\n\nUsage :" + window.location + "?<WebRTC video stream name>")
        }
        return;
    }

    let url = { video:location.search.slice(1) };
    let options;
    if (typeof URLSearchParams != 'undefined') {
        let params = new URLSearchParams(location.search.slice(1));
        if (params.has("video") || params.has("audio")) {
            url = { video:params.get("video"), audio:params.get("audio") };
        }
        options = params.get("options");
    }
    window.onload = function() {
      setupWebRTCProxy({ elemId: 'video', videoUrl: url.video, audioUrl: url.audio, proxyUrl: '', options });
    }
})();

