/* jshint -W117 */
'use strict';

var JSM = require('jingle');
var RTC = require('webrtc-adapter');
var GUM = require('getusermedia');
var GSM = require('getscreenmedia');

var jxt = require('jxt').createRegistry();
jxt.use(require('jxt-xmpp-types'));
jxt.use(require('jxt-xmpp'));

var IqStanza = jxt.getDefinition('iq', 'jabber:client');

(function($) {
   Strophe.addConnectionPlugin('jingle', {
      connection: null,
      peer_constraints: {},
      AUTOACCEPT: false,
      localStream: null,
      manager: null,
      RTC: null,
      getUserMedia: null,
      getScreenMedia: null,

      init: function(conn) {
         var self = this;

         self.RTC = RTC;
         self.getUserMedia = GUM;
         self.getScreenMedia = GSM;

         self.connection = conn;

         var browserDetails = self.RTC.browserDetails;

         if ((browserDetails.version < 33 && browserDetails.browser === 'firefox') || browserDetails.browser === 'chrome') {
            self.peer_constraints = {
               mandatory: {
                  'OfferToReceiveAudio': true,
                  'OfferToReceiveVideo': true
               }
            };

            if (browserDetails.browser === 'firefox') {
               self.peer_constraints.mandatory.MozDontOfferDataChannel = true;
            }
         } else {
            self.peer_constraints = {
               'offerToReceiveAudio': true,
               'offerToReceiveVideo': true
            };

            if (browserDetails.browser === 'firefox') {
               self.peer_constraints.mozDontOfferDataChannel = true;
            }
         }

         self.manager = new JSM({
            peerConnectionConstraints: self.peer_constraints,
            jid: self.connection.jid,
            selfID: self.connection.jid
         });

         var events = {
            'incoming': 'callincoming.jingle',
            'terminated': 'callterminated.jingle',
            'peerStreamAdded': 'remotestreamadded.jingle',
            'peerStreamRemoved': 'remotestreamremoved.jingle',
            'ringing': 'ringing.jingle',
            'log:error': 'error.jingle'
         };

         $.each(events, function(key, val) {
            self.manager.on(key, function() {
               $(document).trigger(val, arguments);
            });
         });

         self.manager.on('incoming', function(session) {
            session.on('change:connectionState', function(session, state) {
               $(document).trigger('iceconnectionstatechange.jingle', [session.sid, session, state]);
            });
         });

         if (this.connection.disco) {
            var capabilities = self.manager.capabilities || [
               'urn:xmpp:jingle:1',
               'urn:xmpp:jingle:apps:rtp:1',
               'urn:xmpp:jingle:apps:rtp:audio',
               'urn:xmpp:jingle:apps:rtp:video',
               'urn:xmpp:jingle:apps:rtp:rtcb-fb:0',
               'urn:xmpp:jingle:apps:rtp:rtp-hdrext:0',
               'urn:xmpp:jingle:apps:rtp:ssma:0',
               'urn:xmpp:jingle:apps:dtls:0',
               'urn:xmpp:jingle:apps:grouping:0',
               'urn:xmpp:jingle:apps:file-transfer:3',
               'urn:xmpp:jingle:transports:ice-udp:1',
               'urn:xmpp:jingle:transports.dtls-sctp:1',
               'urn:ietf:rfc:3264',
               'urn:ietf:rfc:5576',
               'urn:ietf:rfc:5888'
            ];
            var i;
            for (i = 0; i < capabilities.length; i++) {
               self.connection.disco.addFeature(capabilities[i]);
            }
         }
         this.connection.addHandler(this.onJingle.bind(this), 'urn:xmpp:jingle:1', 'iq', 'set', null, null);

         this.manager.on('send', function(data) {

            var iq = new IqStanza(data);

            if (!iq.id) {
               iq.id = self.connection.getUniqueId('sendIQ');
            }

            self.connection.send($.parseXML(iq.toString()).getElementsByTagName('iq')[0]);
         });

         //@TODO add on client unavilable (this.manager.endPeerSessions(peer_jid_full, true))
      },
      onJingle: function(iq) {
         var req = jxt.parse(iq.outerHTML);

         this.manager.process(req.toJSON());

         return true;
      },
      initiate: function(peerjid, stream, offerOptions) { // initiate a new jinglesession to peerjid
         var session = this.manager.createMediaSession(peerjid);

         session.on('change:connectionState', function(session, state) {
            $(document).trigger('iceconnectionstatechange.jingle', [session.sid, session, state]);
         });

         if (stream) {
            this.localStream = stream;
         }

         // configure session
         if (this.localStream) {
            session.addStream(this.localStream);
            session.start(offerOptions);

            return session;
         }

         console.error('No local stream defined');
      },
      terminate: function(jid, reason, silent) { // terminate by sessionid (or all sessions)
         if (typeof jid === 'undefined' || jid === null) {
            this.manager.endAllSessions(reason, silent);
         } else {
            this.manager.endPeerSessions(jid, reason, silent);
         }
      },
      terminateByJid: function(jid) {
         this.manager.endPeerSessions(jid);
      },
      addICEServer: function(server) {
         this.manager.addICEServer(server);
      },
      setICEServers: function(servers) {
         this.manager.iceServers = servers;
      },
      setPeerConstraints: function(constraints) {
         this.manager.config.peerConnectionConstraints = constraints;
      }
   });
}(jQuery));
