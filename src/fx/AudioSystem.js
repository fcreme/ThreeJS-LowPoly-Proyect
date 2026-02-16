import * as THREE from 'three';
import EventBus from '../core/EventBus.js';

export default class AudioSystem {
  constructor(camera) {
    this.listener = new THREE.AudioListener();
    camera.add(this.listener);

    this._sounds = new Map();
    this._loader = new THREE.AudioLoader();
    this._ready = false;
    this._isMoving = false;
    this._lastStepTime = 0;
    this._stepInterval = 0.39; // ~PI/8, matches bobSpeed=8 half-cycle

    this._config = {
      ambient:   { path: 'audio/ambient-forest.mp3', loop: true,  volume: 0.3 },
      footstep:  { path: 'audio/footstep.mp3',       loop: false, volume: 0.4 },
      bookOpen:  { path: 'audio/book-open.mp3',      loop: false, volume: 0.5 },
      bookClose: { path: 'audio/book-close.mp3',     loop: false, volume: 0.5 },
      pageTurn:  { path: 'audio/page-turn.mp3',      loop: false, volume: 0.4 },
      hover:     { path: 'audio/hover.mp3',           loop: false, volume: 0.2 },
    };

    this._setupEventListeners();
  }

  async init() {
    const promises = Object.entries(this._config).map(([name, cfg]) => {
      return this._loadSound(name, cfg).catch((err) => {
        console.warn(`[AudioSystem] Could not load "${name}" from ${cfg.path}`);
      });
    });

    await Promise.allSettled(promises);
    this._ready = true;
  }

  _loadSound(name, config) {
    return new Promise((resolve, reject) => {
      this._loader.load(
        config.path,
        (buffer) => {
          const sound = new THREE.Audio(this.listener);
          sound.setBuffer(buffer);
          sound.setLoop(config.loop);
          sound.setVolume(config.volume);
          this._sounds.set(name, sound);
          resolve();
        },
        undefined,
        reject
      );
    });
  }

  _play(name) {
    if (!this._ready) return;
    const sound = this._sounds.get(name);
    if (!sound) return;
    if (sound.isPlaying) sound.stop();
    sound.play();
  }

  _setupEventListeners() {
    EventBus.on('book:opening', () => this._play('bookOpen'));
    EventBus.on('book:closed', () => this._play('bookClose'));
    EventBus.on('book:pageTurn', () => this._play('pageTurn'));
    EventBus.on('interaction:enter', () => this._play('hover'));

    EventBus.on('player:moved', () => {
      this._isMoving = true;
    });
  }

  startAmbient() {
    const ambient = this._sounds.get('ambient');
    if (ambient && !ambient.isPlaying) {
      ambient.play();
    }
  }

  update(delta) {
    if (!this._ready) return;

    if (this._isMoving) {
      this._lastStepTime += delta;
      if (this._lastStepTime >= this._stepInterval) {
        this._play('footstep');
        this._lastStepTime = 0;
      }
    } else {
      this._lastStepTime = 0;
    }

    this._isMoving = false;
  }

  dispose() {
    for (const sound of this._sounds.values()) {
      if (sound.isPlaying) sound.stop();
      sound.disconnect();
    }
    this._sounds.clear();
  }
}
