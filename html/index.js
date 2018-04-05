import Keyboard from 'novnc/core/input/keyboard';
import Mouse from 'noVNC/core/input/mouse';

window.setupElement = function setupElement(elem, webrtcServer) {
    function onEvent(obj) {
        const message = JSON.stringify(obj);
        if (!webrtcServer.dc) {
            console.log('ignoring message: ', message);
            return;
        }
        console.log('sending message: ', message);
        webrtcServer.dc.send(message);
    }

    const keyboard = new Keyboard(elem);
    keyboard.onkeyevent = (keysym, code, down) => {
        onEvent({
            clicks: [],
            presses: [{
                down,
                code,
                keysym, 
            }],
        });
    };

    const mouse = new Mouse(elem);
    mouse.onmousebutton = (x, y, down, bmask) => {
        onEvent({
            presses: [],
            clicks: [{
                x: Math.floor(1000 * (x / elem.clientWidth)),
                y: Math.floor(1000 * (y / elem.clientHeight)),
                button: down ? bmask : 0,
            }]
        });
    };
    mouse.onmousemove = (x, y) => {
        onEvent({
            presses: [],
            clicks: [{
                x: Math.floor(1000 * (x / elem.clientWidth)),
                y: Math.floor(1000 * (y / elem.clientHeight)),
                button: 0,
            }]
        });
    };
}
