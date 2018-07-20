function move(x, y) {
  return cy.get('video')
    .trigger('mouseover', x, y, {force: true})
}

function click(x, y) {
  return cy.get('video')
    .click(x, y)
  /*
    .trigger('mousedown', x, y, {force: true})
    .trigger('mouseover', x, y, {force: true})
    .trigger('mouseup', x, y, {force: true})
    */
}
function clickForce(x, y) {
  return cy.get('video')
    .click(x, y, { force: true })
  /*
    .trigger('mousedown', x, y, {force: true})
    .trigger('mouseover', x, y, {force: true})
    .trigger('mouseup', x, y, {force: true})
    */
}

function guid() {
  function s4() {
    return Math.floor((1 + Math.random()) * 0x10000)
      .toString(16)
      .substring(1);
  }
  return s4() + s4() + '-' + s4() + '-' + s4() + '-' + s4() + '-' + s4() + s4() + s4();
}

function type(txt) {
  /*
  let elem = cy.get('video')
  txt.split('').forEach((ch, index) => {
    elem = elem.trigger('keydown', { keyCode: txt.charCodeAt(index), which: txt.charCodeAt(index), force: true })
  })
  */
 return cy.get('body').type(txt, { force: true })
}

function typeDelay(txt) {
 return cy.get('body').type(txt, { force: true, delay: 200 })
}

function visitPage(url) {
  move(500, 50) 
  click(500, 50) 
  type('{backspace}')
  type(`${url}`)
  type('{shift}/')
  return type('{shift}?/?{enter}')
}

function startInteraction(socket) {
  /*
  move(150, 500)
  clickForce(150, 500)
  */

  move(150, 190).then(() => {
    clickForce(150, 190);
    socket.send(JSON.stringify({
      target: 'click-me',
      value: 'clicked',
      sent: new Date(),
    }))
  });

  move(150, 240).then(() => {
    socket.send(JSON.stringify({
      target: 'me-too',
      value: 'clicked',
      sent: new Date(),
    }))
    clickForce(150, 240);
  })

  move(150, 320)
  const str = 'Hello World';
  typeDelay(str).then(() => {
    str.split('').forEach((ch, index) => {
      socket.send(JSON.stringify({
          target: 'input',
          value: str.slice(index),
          sent: new Date(),
        }))
    })
  });
  clickForce(150, 320)
}

let pendingMessage, resolveMessage, rejectMessage;
function resetPendingMessage() {
  pendingMessage = new Promise((resolve, reject) => {
    resolveMessage = resolve;
    rejectMessage = reject;
  }).then(() => {
    resetPendingMessage();
  });

  return pendingMessage;
}

function setupPendingMessage() {
  if (!pendingMessage) {
    resetPendingMessage();
  }
}

function getPendingMessage() {
  if (!pendingMessage) {
    return resetPendingMessage(); 
  }

  return pendingMessage;
}

describe('benchmarking proxy', () => {
  it('interacts properly', () => {
    const session =  guid();
    const socket = new WebSocket(`ws://webrtc-streamer-test.herokuapp.com/api?session=${session}`);
    setupPendingMessage();
    socket.onmessage = message => {
      try {
        const obj = JSON.parse(message);
        resolveMessage(obj);
      }
      catch(ex) {
        rejectMessage(ex);
      }
    }

    cy.visit(`${Cypress.env('PROXY_URL')}/index.html?video=${Cypress.env('VNC_URL')}`)
    cy.get('video', { timeout: 100000 }).invoke('width').should('be.gt', 1000)

    /* 
    visitPage(`webrtc-streamer-test.herokuapp.com/test?session=${session}`)
      .then(() => getPendingMessage())
      .then(message => console.log(message));
    */

    startInteraction(socket)
  });
});