import * as THREE from 'three';
import EventBus from './EventBus.js';
import PostProcessing from '../fx/PostProcessing.js';
import { PSX_RENDER_HEIGHT, PSX_FOG_DENSITY, PSX_CAMERA_FAR, PSX_EXPOSURE } from '../fx/PSXEffect.js';

export default class Engine {
  constructor(container) {
    const aspect = window.innerWidth / window.innerHeight;
    const psxW = Math.round(PSX_RENDER_HEIGHT * aspect);

    this.renderer = new THREE.WebGLRenderer({ antialias: false });
    this.renderer.setSize(psxW, PSX_RENDER_HEIGHT, false);
    this.renderer.setPixelRatio(1);
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = PSX_EXPOSURE;
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;

    this.renderer.domElement.style.width = '100%';
    this.renderer.domElement.style.height = '100%';
    this.renderer.domElement.style.imageRendering = 'auto';
    (container || document.body).appendChild(this.renderer.domElement);

    this.scene = new THREE.Scene();
    this.scene.fog = new THREE.FogExp2(0x1a1510, PSX_FOG_DENSITY);

    this.camera = new THREE.PerspectiveCamera(65, aspect, 0.1, PSX_CAMERA_FAR);
    this.camera.position.set(0, 1.6, 0);

    this.postProcessing = new PostProcessing(this.renderer, this.scene, this.camera);

    this.clock = new THREE.Clock();
    this._running = false;

    window.addEventListener('resize', () => this._onResize());
  }

  _onResize() {
    const aspect = window.innerWidth / window.innerHeight;
    const psxW = Math.round(PSX_RENDER_HEIGHT * aspect);
    this.camera.aspect = aspect;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(psxW, PSX_RENDER_HEIGHT, false);
    this.postProcessing.resize(psxW, PSX_RENDER_HEIGHT);
  }

  start(updateCallback) {
    this._running = true;
    this._updateCallback = updateCallback;
    this._loop();
  }

  stop() {
    this._running = false;
  }

  _loop() {
    if (!this._running) return;
    requestAnimationFrame(() => this._loop());
    const delta = this.clock.getDelta();
    const elapsed = this.clock.getElapsedTime();
    if (this._updateCallback) {
      this._updateCallback(delta, elapsed);
    }
    EventBus.emit('engine:update', { delta, elapsed });
    this.postProcessing.update(elapsed);
    this.postProcessing.render();
  }
}
