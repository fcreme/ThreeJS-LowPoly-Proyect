import * as THREE from 'three';
import { randomRange } from '../utils/math.js';

export default class Fireflies {
  constructor(scene) {
    this.scene = scene;
    this.points = null;
    this._data = [];
    this._pathCurve = null;
  }

  build(pathCurve) {
    this._pathCurve = pathCurve;
    const count = 40;
    const positions = new Float32Array(count * 3);
    const colors = new Float32Array(count * 3);
    this._data = [];

    const baseColor = new THREE.Color(0xaaff44);

    for (let i = 0; i < count; i++) {
      const pathT = Math.random();
      const perpOffset = randomRange(-2, 2);
      const height = randomRange(0.5, 2.5);
      const phase = Math.random() * Math.PI * 2;
      const driftSpeed = randomRange(0.002, 0.008);

      this._data.push({ pathT, perpOffset, height, phase, driftSpeed });

      const pos = this._computePosition(pathT, perpOffset, height);
      positions[i * 3] = pos.x;
      positions[i * 3 + 1] = pos.y;
      positions[i * 3 + 2] = pos.z;

      const brightness = randomRange(0.7, 1.0);
      colors[i * 3] = baseColor.r * brightness;
      colors[i * 3 + 1] = baseColor.g * brightness;
      colors[i * 3 + 2] = baseColor.b * brightness;
    }

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

    const material = new THREE.PointsMaterial({
      size: 0.15,
      vertexColors: true,
      transparent: true,
      opacity: 0.8,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
      sizeAttenuation: true,
      fog: false,
    });

    this.points = new THREE.Points(geometry, material);
    this.points.name = 'fireflies';
    this.scene.add(this.points);
  }

  _computePosition(pathT, perpOffset, height) {
    const t = Math.max(0, Math.min(1, pathT));
    const point = this._pathCurve.getPointAt(t);
    const tangent = this._pathCurve.getTangentAt(t);

    const perp = new THREE.Vector3()
      .crossVectors(tangent, new THREE.Vector3(0, 1, 0))
      .normalize();

    return new THREE.Vector3(
      point.x + perp.x * perpOffset,
      height,
      point.z + perp.z * perpOffset
    );
  }

  update(delta, elapsed) {
    if (!this.points) return;

    const posArr = this.points.geometry.attributes.position.array;

    for (let i = 0; i < this._data.length; i++) {
      const d = this._data[i];
      const i3 = i * 3;

      // Drift along path
      d.pathT += d.driftSpeed * delta;
      if (d.pathT > 1) d.pathT -= 1;

      // Oscillate perpendicular offset and height
      const perpOsc = d.perpOffset + Math.sin(elapsed * 0.5 + d.phase) * 0.5;
      const heightOsc = d.height + Math.sin(elapsed * 0.3 + d.phase * 1.3) * 0.3;

      const pos = this._computePosition(d.pathT, perpOsc, heightOsc);
      posArr[i3] = pos.x;
      posArr[i3 + 1] = pos.y;
      posArr[i3 + 2] = pos.z;
    }

    this.points.geometry.attributes.position.needsUpdate = true;

    // Pulse overall opacity
    this.points.material.opacity = 0.5 + Math.sin(elapsed * 1.5) * 0.3;
  }

  dispose() {
    if (this.points) {
      this.points.geometry.dispose();
      this.points.material.dispose();
      this.scene.remove(this.points);
    }
  }
}
