export default class LoadingScreen {
  constructor() {
    this._screen = document.getElementById('loading-screen');
    this._bar = document.getElementById('loading-bar');
    this._text = document.getElementById('loading-text');
  }

  setProgress(value, text) {
    const pct = Math.min(100, Math.max(0, value * 100));
    this._bar.style.width = `${pct}%`;
    if (text) this._text.textContent = text;
  }

  hide() {
    this._screen.classList.add('hidden');
    setTimeout(() => {
      this._screen.style.display = 'none';
    }, 800);
  }
}
