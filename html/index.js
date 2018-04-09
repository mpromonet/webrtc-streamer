import Keyboard from '@novnc/novnc/core/input/keyboard';
import Mouse from '@novnc/novnc/core/input/mouse';

let pendingEvents = [];
let sendEventTimeout = null;
function flushEvents(webrtcServer) {
    clearTimeout(sendEventTimeout);
    sendEventTimeout = setTimeout(flushEvents.bind(null, webrtcServer), 200);
    const events = filteredEvents(pendingEvents);
    pendingEvents = [];
    if (!events.length) {
        console.log('no pending events in this time slot');
        return;
    }
    const message = JSON.stringify({ events });
    if (!webrtcServer.dc) {
        console.log('ignoring message: ', message);
        return;
    }
    
    console.log('sending message: ', message);
    webrtcServer.dc.send(message);
}

/*
    Filter out all the mouse move events in between mouse button states.
    e.g if first starts from (10, 20) then hovers to (10, 30), then clicks and drags
    until (10, 40) then the filteredEvents will only keep events:
    1. (10, 20) with button state static
    2. (10, 30) with button state static
    3. (10, 30) with button state dragged
    4. (10, 40) with button state dragged
*/
function filteredEvents(pendingEvents) {
    let skippedEvs = 0;
    const { currentClick, events: evs } = pendingEvents.reduce(({ currentClick, events }, ev) => {
        let newEvents = events;
        if (ev.isClick) {
            if (!currentClick) {
                newEvents = [...newEvents, ev];
            }
            else if (currentClick.button !== ev.button) {
                if (skippedEvs) {
                    newEvents = [...newEvents, currentClick];
                }
            }
            else {
                skippedEvs++;
            }

            return {
                currentClick: ev,
                events: newEvents,
            }
        }

        return { currentClick, events: [...events, ev] };
    }, { currentClick: null, events: [] });
    return currentClick ? [...evs, currentClick] : evs;
}

function onEvent(webrtcServer, event) {
    pendingEvents.push(event);
    // respond instantly when a keyboard press, mouse button press or initial event
    if (event.isPress || !sendEventTimeout || event.button) {
        flushEvents(webrtcServer);
    }
}

const onMouseEvent = (webrtcServer, clickEvent) => onEvent(webrtcServer, {
    ...clickEvent,
    isClick: true,
});

const onKeyboardEvent = (webrtcServer, keyEvent) => onEvent(webrtcServer, {
    ...keyEvent,
    isPress: true,
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
