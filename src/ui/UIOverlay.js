import EventBus from '../core/EventBus.js';

export default class UIOverlay {
  constructor() {
    this._prompt = document.getElementById('interaction-prompt');
    this._bookControls = document.getElementById('book-controls');
    this._clickToStart = document.getElementById('click-to-start');

    this._setupEvents();
  }

  _setupEvents() {
    EventBus.on('interaction:enter', ({ target }) => {
      if (target.userData.promptText) {
        this.showPrompt(target.userData.promptText);
      }
    });

    EventBus.on('interaction:exit', () => {
      this.hidePrompt();
    });

    EventBus.on('book:open', () => {
      this.hidePrompt();
      this.showBookControls();
    });

    EventBus.on('book:closed', () => {
      this.hideBookControls();
    });

    EventBus.on('player:locked', () => {
      this._clickToStart.classList.add('hidden');
    });

    EventBus.on('player:unlocked', () => {
      this._clickToStart.classList.remove('hidden');
    });
  }

  showPrompt(text) {
    this._prompt.textContent = text;
    this._prompt.classList.add('visible');
  }

  hidePrompt() {
    this._prompt.classList.remove('visible');
  }

  showBookControls() {
    this._bookControls.classList.add('visible');
  }

  hideBookControls() {
    this._bookControls.classList.remove('visible');
  }
}
