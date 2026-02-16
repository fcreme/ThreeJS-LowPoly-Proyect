import * as THREE from 'three';
import { randomRange } from '../utils/math.js';
import { patchMaterial } from '../fx/PSXEffect.js';

export default class ForestGenerator {
  constructor(scene) {
    this.scene = scene;
    this.instancedMeshes = [];
    this._colliders = [];
  }

  build(pathCurve) {
    const treeCount = 300;
    const variants = this._createTreeVariants();

    // Distribute counts across variants
    const counts = [
      Math.floor(treeCount * 0.4),  // tall thin pines
      Math.floor(treeCount * 0.35), // medium dead trees
      treeCount - Math.floor(treeCount * 0.4) - Math.floor(treeCount * 0.35), // gnarled
    ];

    // Generate positions avoiding the path
    const pathSamplePoints = [];
    for (let t = 0; t <= 1; t += 0.005) {
      pathSamplePoints.push(pathCurve.getPointAt(t));
    }

    for (let v = 0; v < variants.length; v++) {
      const { geometry, material } = variants[v];
      const count = counts[v];
      const instanced = new THREE.InstancedMesh(geometry, material, count);
      instanced.castShadow = true;
      instanced.receiveShadow = true;
      instanced.name = `trees-variant-${v}`;

      const dummy = new THREE.Object3D();
      let placed = 0;

      while (placed < count) {
        const x = randomRange(-90, 90);
        const z = randomRange(-90, 90);

        // Check distance from path
        let tooClose = false;
        for (const pp of pathSamplePoints) {
          const dx = x - pp.x;
          const dz = z - pp.z;
          if (dx * dx + dz * dz < 12) { // ~3.5 unit radius
            tooClose = true;
            break;
          }
        }
        if (tooClose) continue;

        dummy.position.set(x, 0, z);
        dummy.rotation.y = Math.random() * Math.PI * 2;
        const scale = randomRange(0.7, 1.3);
        dummy.scale.set(scale, scale * randomRange(0.8, 1.2), scale);
        dummy.updateMatrix();
        instanced.setMatrixAt(placed, dummy.matrix);
        placed++;
      }

      instanced.instanceMatrix.needsUpdate = true;
      this.scene.add(instanced);
      this.instancedMeshes.push(instanced);
    }

    // Create invisible collision cylinders for trees nearest to the path
    this._createColliders(pathCurve);

    return this._colliders;
  }

  _createTreeVariants() {
    // Variant 0: Tall pine tree
    const pine = this._createPineGeometry();
    const pineMat = new THREE.MeshLambertMaterial({ color: 0x3a5540 });
    patchMaterial(pineMat);

    // Variant 1: Dead tree (trunk only, no leaves)
    const dead = this._createDeadTreeGeometry();
    const deadMat = new THREE.MeshLambertMaterial({ color: 0x5a4535 });
    patchMaterial(deadMat);

    // Variant 2: Gnarled thick tree
    const gnarled = this._createGnarledGeometry();
    const gnarledMat = new THREE.MeshLambertMaterial({ color: 0x4a3e30 });
    patchMaterial(gnarledMat);

    return [
      { geometry: pine, material: pineMat },
      { geometry: dead, material: deadMat },
      { geometry: gnarled, material: gnarledMat },
    ];
  }

  _createPineGeometry() {
    const group = new THREE.BufferGeometry();

    // Trunk
    const trunk = new THREE.CylinderGeometry(0.12, 0.18, 6, 6);
    trunk.translate(0, 3, 0);

    // Foliage layers
    const foliage1 = new THREE.ConeGeometry(1.8, 3, 6);
    foliage1.translate(0, 5.5, 0);

    const foliage2 = new THREE.ConeGeometry(1.4, 2.5, 6);
    foliage2.translate(0, 7, 0);

    const foliage3 = new THREE.ConeGeometry(0.9, 2, 6);
    foliage3.translate(0, 8.2, 0);

    return this._mergeGeometries([trunk, foliage1, foliage2, foliage3]);
  }

  _createDeadTreeGeometry() {
    // Trunk with taper
    const trunk = new THREE.CylinderGeometry(0.08, 0.22, 5, 5);
    trunk.translate(0, 2.5, 0);

    // Branches (angled cylinders)
    const branch1 = new THREE.CylinderGeometry(0.03, 0.06, 2, 4);
    branch1.rotateZ(Math.PI / 4);
    branch1.translate(0.8, 3.5, 0);

    const branch2 = new THREE.CylinderGeometry(0.03, 0.05, 1.5, 4);
    branch2.rotateZ(-Math.PI / 3);
    branch2.translate(-0.6, 4, 0.2);

    return this._mergeGeometries([trunk, branch1, branch2]);
  }

  _createGnarledGeometry() {
    const trunk = new THREE.CylinderGeometry(0.15, 0.35, 4, 6);
    trunk.translate(0, 2, 0);

    // Thick canopy blob
    const canopy = new THREE.SphereGeometry(2, 6, 5);
    canopy.translate(0, 4.5, 0);
    canopy.scale(1, 0.7, 1);

    return this._mergeGeometries([trunk, canopy]);
  }

  _mergeGeometries(geometries) {
    // Manual merge: combine all positions, normals, indices
    let totalVerts = 0;
    let totalIdx = 0;
    for (const g of geometries) {
      totalVerts += g.attributes.position.count;
      totalIdx += g.index ? g.index.count : 0;
    }

    const positions = new Float32Array(totalVerts * 3);
    const normals = new Float32Array(totalVerts * 3);
    const indices = [];

    let vertOffset = 0;
    let idxOffset = 0;

    for (const g of geometries) {
      const pos = g.attributes.position;
      const norm = g.attributes.normal;

      for (let i = 0; i < pos.count; i++) {
        positions[(vertOffset + i) * 3] = pos.getX(i);
        positions[(vertOffset + i) * 3 + 1] = pos.getY(i);
        positions[(vertOffset + i) * 3 + 2] = pos.getZ(i);

        if (norm) {
          normals[(vertOffset + i) * 3] = norm.getX(i);
          normals[(vertOffset + i) * 3 + 1] = norm.getY(i);
          normals[(vertOffset + i) * 3 + 2] = norm.getZ(i);
        }
      }

      if (g.index) {
        for (let i = 0; i < g.index.count; i++) {
          indices.push(g.index.getX(i) + vertOffset);
        }
      }

      vertOffset += pos.count;
      g.dispose();
    }

    const merged = new THREE.BufferGeometry();
    merged.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    merged.setAttribute('normal', new THREE.BufferAttribute(normals, 3));
    if (indices.length > 0) {
      merged.setIndex(indices);
    }

    return merged;
  }

  _createColliders(pathCurve) {
    // Store tree positions as {x, z, radius} for fast distance-based collision
    // Only trees within 10 units of the path
    const pathSamples = [];
    for (let t = 0; t <= 1; t += 0.02) {
      pathSamples.push(pathCurve.getPointAt(t));
    }

    const matrix = new THREE.Matrix4();
    const position = new THREE.Vector3();

    for (const instanced of this.instancedMeshes) {
      for (let i = 0; i < instanced.count; i++) {
        instanced.getMatrixAt(i, matrix);
        position.setFromMatrixPosition(matrix);

        let nearPath = false;
        for (const pp of pathSamples) {
          const dx = position.x - pp.x;
          const dz = position.z - pp.z;
          if (dx * dx + dz * dz < 100) {
            nearPath = true;
            break;
          }
        }

        if (nearPath) {
          this._colliders.push({ x: position.x, z: position.z, radius: 0.6 });
        }
      }
    }
  }

  get colliders() {
    return this._colliders;
  }

  dispose() {
    for (const m of this.instancedMeshes) {
      m.geometry.dispose();
      m.material.dispose();
      this.scene.remove(m);
    }
    this.instancedMeshes = [];
    this._colliders = [];
  }
}
