import * as THREE from 'three';
import { randomRange } from '../utils/math.js';

export default class ParticleSystem {
  constructor(scene) {
    this.scene = scene;
    this.points = null;
    this._velocities = [];
  }

  build(playerPosition) {
    const count = 500;
    const positions = new Float32Array(count * 3);
    const colors = new Float32Array(count * 3);
    const sizes = new Float32Array(count);
    this._velocities = [];

    for (let i = 0; i < count; i++) {
      positions[i * 3] = randomRange(-40, 40);
      positions[i * 3 + 1] = randomRange(0.5, 6);
      positions[i * 3 + 2] = randomRange(-40, 40);

      // Mix 60% cold blue-tinted motes + 40% warm amber
      const brightness = randomRange(0.2, 0.5);
      if (Math.random() < 0.6) {
        // Cold blue mote
        colors[i * 3] = brightness * 0.5;
        colors[i * 3 + 1] = brightness * 0.6;
        colors[i * 3 + 2] = brightness;
      } else {
        // Warm amber mote
        colors[i * 3] = brightness;
        colors[i * 3 + 1] = brightness * 0.7;
        colors[i * 3 + 2] = brightness * 0.3;
      }

      sizes[i] = randomRange(1.5, 4.0);

      this._velocities.push({
        x: randomRange(-0.02, 0.02),
        y: randomRange(-0.005, 0.01),
        z: randomRange(-0.02, 0.02),
        phase: Math.random() * Math.PI * 2,
      });
    }

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
    geometry.setAttribute('size', new THREE.BufferAttribute(sizes, 1));

    const material = new THREE.PointsMaterial({
      size: 0.05,
      vertexColors: true,
      transparent: true,
      opacity: 0.25,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
      sizeAttenuation: true,
    });

    this.points = new THREE.Points(geometry, material);
    this.points.name = 'dustMotes';
    this.scene.add(this.points);
  }

  update(delta, elapsed, playerPosition) {
    if (!this.points) return;

    const positions = this.points.geometry.attributes.position;
    const arr = positions.array;

    for (let i = 0; i < positions.count; i++) {
      const vel = this._velocities[i];
      const i3 = i * 3;

      // Gentle drift
      arr[i3] += vel.x * delta * 30 + Math.sin(elapsed + vel.phase) * 0.002;
      arr[i3 + 1] += vel.y * delta * 30 + Math.sin(elapsed * 0.5 + vel.phase) * 0.003;
      arr[i3 + 2] += vel.z * delta * 30 + Math.cos(elapsed * 0.7 + vel.phase) * 0.002;

      // Wrap around player
      if (playerPosition) {
        const dx = arr[i3] - playerPosition.x;
        const dz = arr[i3 + 2] - playerPosition.z;
        if (Math.abs(dx) > 40) arr[i3] = playerPosition.x + randomRange(-30, 30);
        if (Math.abs(dz) > 40) arr[i3 + 2] = playerPosition.z + randomRange(-30, 30);
      }

      // Keep in vertical range
      if (arr[i3 + 1] < 0.2) arr[i3 + 1] = randomRange(3, 6);
      if (arr[i3 + 1] > 8) arr[i3 + 1] = randomRange(0.5, 2);
    }

    positions.needsUpdate = true;
  }

  dispose() {
    if (this.points) {
      this.points.geometry.dispose();
      this.points.material.dispose();
      this.scene.remove(this.points);
    }
  }
}
