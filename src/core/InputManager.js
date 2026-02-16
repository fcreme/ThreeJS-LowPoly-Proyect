import EventBus from './EventBus.js';

export default class InputManager {
  constructor() {
    this.keys = {};
    this._onKeyDown = this._onKeyDown.bind(this);
    this._onKeyUp = this._onKeyUp.bind(this);

    window.addEventListener('keydown', this._onKeyDown);
    window.addEventListener('keyup', this._onKeyUp);
  }

  _onKeyDown(e) {
    if (this.keys[e.code]) return; // already held
    this.keys[e.code] = true;
    EventBus.emit('input:keydown', e.code);
  }

  _onKeyUp(e) {
    this.keys[e.code] = false;
    EventBus.emit('input:keyup', e.code);
  }

  isPressed(code) {
    return !!this.keys[code];
  }

  dispose() {
    window.removeEventListener('keydown', this._onKeyDown);
    window.removeEventListener('keyup', this._onKeyUp);
  }
}
