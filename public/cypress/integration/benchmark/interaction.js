function move(x, y) {
    cy.get('video')
      .trigger('mouseover', x, y, {force: true})
}

describe('benchmarking proxy', () => {
  it('interacts properly', () => {
    cy.visit(`${Cypress.env('PROXY_URL')}/index.html?video=${Cypress.env('VNC_URL')}`)
    cy.get('video', { timeout: 100000 }).invoke('width').should('be.gt', 1000)

    cy.get('video').click(500, 50, {force: true}) 
    cy.get('video').type('www.murcul.com{enter}', { force: true })
  });
});