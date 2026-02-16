import * as THREE from 'three';
import { patchMaterial } from '../fx/PSXEffect.js';

export default class TerrainBuilder {
  constructor(scene) {
    this.scene = scene;
    this.mesh = null;
  }

  build() {
    const geometry = new THREE.PlaneGeometry(200, 200, 64, 64);
    geometry.rotateX(-Math.PI / 2);

    // Add subtle height variation
    const positions = geometry.attributes.position;
    for (let i = 0; i < positions.count; i++) {
      const x = positions.getX(i);
      const z = positions.getZ(i);
      const y = Math.sin(x * 0.05) * Math.cos(z * 0.05) * 0.4
              + Math.sin(x * 0.15 + z * 0.1) * 0.15
              + Math.sin(x * 0.4 + z * 0.3) * 0.05;
      positions.setY(i, y);
    }
    geometry.computeVertexNormals();

    // Vertex color variation — breaks up the flat single-color look
    const colors = new Float32Array(positions.count * 3);
    const baseColor = new THREE.Color(0x5a4a30);
    for (let i = 0; i < positions.count; i++) {
      const x = positions.getX(i);
      const z = positions.getZ(i);
      // Perlin-like variation using layered sin
      const noise = Math.sin(x * 0.3 + z * 0.2) * 0.1
                  + Math.sin(x * 0.7 - z * 0.5) * 0.06
                  + Math.sin(x * 1.5 + z * 1.2) * 0.04;
      const r = Math.max(0, Math.min(1, baseColor.r + noise));
      const g = Math.max(0, Math.min(1, baseColor.g + noise * 0.8));
      const b = Math.max(0, Math.min(1, baseColor.b + noise * 0.5));
      colors[i * 3] = r;
      colors[i * 3 + 1] = g;
      colors[i * 3 + 2] = b;
    }
    geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

    const material = new THREE.MeshLambertMaterial({ color: 0xffffff, vertexColors: true });
    patchMaterial(material);

    this.mesh = new THREE.Mesh(geometry, material);
    this.mesh.receiveShadow = true;
    this.mesh.name = 'terrain';
    this.scene.add(this.mesh);

    return this.mesh;
  }

  dispose() {
    if (this.mesh) {
      this.mesh.geometry.dispose();
      this.mesh.material.dispose();
      this.scene.remove(this.mesh);
    }
  }
}
