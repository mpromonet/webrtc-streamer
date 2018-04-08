import Keyboard from '@novnc/novnc/core/input/keyboard';
import Mouse from '@novnc/novnc/core/input/mouse';

function onEvent(webrtcServer, obj) {
    const message = JSON.stringify(obj);
    if (!webrtcServer.dc) {
        console.log('ignoring message: ', message);
        return;
    }
    console.log('sending message: ', message);
    webrtcServer.dc.send(message);
}

const onMouseEvent = (webrtcServer, clickEvent) => onEvent(webrtcServer, {
    presses: [],
    clicks: [clickEvent],
});

const onKeyboardEvent = (webrtcServer, keyEvent) => onEvent(webrtcServer, {
    clicks: [],
    presses: [keyEvent],
});

window.setupElement = function setupElement(elem, webrtcServer) {
    console.log('setting up webrtc element!', elem, webrtcServer);
    // Mouse button state
    let buttonMask = 0;

    const keyboard = new Keyboard(document);
    keyboard.onkeyevent = (keysym, code, down) => {
        console.log('got back keyboard event: ', keysym, code, down);
        onKeyboardEvent(webrtcServer, {
            down,
            code: keysym,
        });
    };

    const mouse = new Mouse(elem);
    mouse.onmousebutton = (x, y, down, bmask) => {
        console.log('got back mouse button: ', x, y, down, bmask);
        if (down) {
            buttonMask |= bmask;
        } else {
            buttonMask &= ~bmask;
        }

        const relX = Math.floor(1000 * (x / elem.clientWidth));
        const relY = Math.floor(1000 * (y / elem.clientHeight));

        onMouseEvent(webrtcServer, { x: relX, y: relY, button: buttonMask });
    };

    mouse.onmousemove = (x, y) => {
        console.log('on mouse move: ', x, y);
        const relX = Math.floor(1000 * (x / elem.clientWidth));
        const relY = Math.floor(1000 * (y / elem.clientHeight));
        onMouseEvent(webrtcServer, { x: relX, y: relY, button: buttonMask });
    };

    mouse.grab();
    keyboard.grab();
    console.log('grabbed keyboard successfully!: ', keyboard);
}
