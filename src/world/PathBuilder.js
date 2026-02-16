import * as THREE from 'three';
import { patchMaterial } from '../fx/PSXEffect.js';

export default class PathBuilder {
  constructor(scene) {
    this.scene = scene;
    this.curve = null;
    this.mesh = null;
  }

  build() {
    // Define control points for a winding forest path
    const points = [
      new THREE.Vector3(0, 0.01, 40),
      new THREE.Vector3(-5, 0.01, 25),
      new THREE.Vector3(3, 0.01, 10),
      new THREE.Vector3(-2, 0.01, -5),
      new THREE.Vector3(6, 0.01, -20),
      new THREE.Vector3(0, 0.01, -35),
      new THREE.Vector3(-8, 0.01, -50),
      new THREE.Vector3(2, 0.01, -65),
      new THREE.Vector3(-3, 0.01, -80),
    ];

    this.curve = new THREE.CatmullRomCurve3(points, false, 'catmullrom', 0.5);

    // Create path mesh — extruded flat ribbon
    const pathPoints = this.curve.getPoints(200);
    const shape = new THREE.Shape();
    shape.moveTo(-1.5, 0);
    shape.lineTo(1.5, 0);
    shape.lineTo(1.5, 0.02);
    shape.lineTo(-1.5, 0.02);
    shape.lineTo(-1.5, 0);

    // Build path as a series of planes along the curve
    const geometry = new THREE.BufferGeometry();
    const vertices = [];
    const uvs = [];
    const indices = [];

    for (let i = 0; i < pathPoints.length - 1; i++) {
      const p0 = pathPoints[i];
      const p1 = pathPoints[i + 1];

      const forward = new THREE.Vector3().subVectors(p1, p0).normalize();
      const right = new THREE.Vector3().crossVectors(forward, new THREE.Vector3(0, 1, 0)).normalize();

      const width = 1.8;
      const halfW = width / 2;

      const v0 = new THREE.Vector3().copy(p0).addScaledVector(right, -halfW);
      const v1 = new THREE.Vector3().copy(p0).addScaledVector(right, halfW);
      const v2 = new THREE.Vector3().copy(p1).addScaledVector(right, -halfW);
      const v3 = new THREE.Vector3().copy(p1).addScaledVector(right, halfW);

      v0.y = 0.02;
      v1.y = 0.02;
      v2.y = 0.02;
      v3.y = 0.02;

      const base = (i * 2);
      const t0 = i / (pathPoints.length - 1);
      const t1 = (i + 1) / (pathPoints.length - 1);

      vertices.push(v0.x, v0.y, v0.z);
      vertices.push(v1.x, v1.y, v1.z);
      uvs.push(0, t0 * 20);
      uvs.push(1, t0 * 20);

      if (i === pathPoints.length - 2) {
        vertices.push(v2.x, v2.y, v2.z);
        vertices.push(v3.x, v3.y, v3.z);
        uvs.push(0, t1 * 20);
        uvs.push(1, t1 * 20);
      }
    }

    // Add closing vertices
    for (let i = 0; i < pathPoints.length - 1; i++) {
      const base = i * 2;
      indices.push(base, base + 1, base + 2);
      indices.push(base + 1, base + 3, base + 2);
    }

    geometry.setAttribute('position', new THREE.Float32BufferAttribute(vertices, 3));
    geometry.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
    geometry.setIndex(indices);
    geometry.computeVertexNormals();

    const material = new THREE.MeshLambertMaterial({ color: 0x7a6848 });
    patchMaterial(material);

    this.mesh = new THREE.Mesh(geometry, material);
    this.mesh.receiveShadow = true;
    this.mesh.name = 'path';
    this.scene.add(this.mesh);

    return { curve: this.curve, mesh: this.mesh };
  }

  getPointAtT(t) {
    if (!this.curve) return new THREE.Vector3();
    return this.curve.getPointAt(Math.max(0, Math.min(1, t)));
  }

  getTangentAtT(t) {
    if (!this.curve) return new THREE.Vector3(0, 0, -1);
    return this.curve.getTangentAt(Math.max(0, Math.min(1, t)));
  }

  dispose() {
    if (this.mesh) {
      this.mesh.geometry.dispose();
      this.mesh.material.dispose();
      this.scene.remove(this.mesh);
    }
  }
}
