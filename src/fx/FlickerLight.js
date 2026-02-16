import * as THREE from 'three';

export default class FlickerLight {
  constructor(scene, position, color = 0xff8844, intensity = 1.5, distance = 12) {
    this.light = new THREE.PointLight(color, intensity, distance);
    this.light.position.copy(position);
    // No shadows on point lights — performance budget
    this.light.castShadow = false;
    scene.add(this.light);

    this._baseIntensity = intensity;
    this._phase = Math.random() * Math.PI * 2;
    this._speed = 2 + Math.random() * 3;
    this._flicker = 0.3 + Math.random() * 0.2;
  }

  update(elapsed) {
    const t = elapsed;
    const p = this._phase;
    const s = this._speed;

    // Medium: existing dual sine
    const medium = Math.sin(t * s + p) * Math.sin(t * s * 1.7 + p * 2) * this._flicker;
    // Fast flicker
    const fast = Math.sin(t * s * 3 + p * 1.3) * 0.15;
    // Slow pulse
    const slow = Math.sin(t * s * 0.5 + p * 0.7) * 0.1;
    // Occasional sharp dip (simulates draft)
    const dip = Math.pow(Math.sin(t * 0.8 + p), 16) * -0.6;

    this.light.intensity = Math.max(0.1, this._baseIntensity + medium + fast + slow + dip);
  }

  dispose(scene) {
    scene.remove(this.light);
    this.light.dispose();
  }
}
