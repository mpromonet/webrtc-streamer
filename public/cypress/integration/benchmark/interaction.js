describe('benchmarking proxy', () => {
  it('interacts properly', () => {
    cy.visit(`${Cypress.env('PROXY_URL')}/index.html?video=${Cypress.env('VNC_URL')}`)
    cy.get('video', { timeout: 100000 }).invoke('width').should('be.gt', 1000)
  });
});