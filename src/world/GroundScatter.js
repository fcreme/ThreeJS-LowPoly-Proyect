import * as THREE from 'three';
import { randomRange } from '../utils/math.js';
import { patchMaterial } from '../fx/PSXEffect.js';

export default class GroundScatter {
  constructor(scene) {
    this.scene = scene;
    this.instancedMeshes = [];
  }

  build(pathCurve, treeColliders) {
    const pathSamples = [];
    for (let t = 0; t <= 1; t += 0.01) {
      pathSamples.push(pathCurve.getPointAt(t));
    }

    this._buildGrass(pathSamples, treeColliders);
    this._buildLeaves(pathSamples, treeColliders);
    this._buildRocks(pathSamples, treeColliders);
  }

  _placeInstances(instanced, count, pathSamples, treeColliders, maxDist, callback) {
    const dummy = new THREE.Object3D();
    let placed = 0;
    let attempts = 0;
    const maxAttempts = count * 10;

    while (placed < count && attempts < maxAttempts) {
      attempts++;
      const sample = pathSamples[Math.floor(Math.random() * pathSamples.length)];

      const angle = Math.random() * Math.PI * 2;
      const dist = randomRange(1.5, maxDist);
      const x = sample.x + Math.cos(angle) * dist;
      const z = sample.z + Math.sin(angle) * dist;

      let blocked = false;
      for (const tree of treeColliders) {
        const dx = x - tree.x;
        const dz = z - tree.z;
        if (dx * dx + dz * dz < 1.0) {
          blocked = true;
          break;
        }
      }
      if (blocked) continue;

      callback(dummy, x, z, placed);
      dummy.updateMatrix();
      instanced.setMatrixAt(placed, dummy.matrix);
      placed++;
    }

    instanced.count = placed;
    instanced.instanceMatrix.needsUpdate = true;
  }

  _buildGrass(pathSamples, treeColliders) {
    // Crossed planes (X shape) for grass tufts
    const plane1 = new THREE.PlaneGeometry(0.15, 0.25);
    const plane2 = new THREE.PlaneGeometry(0.15, 0.25);
    plane2.rotateY(Math.PI / 2);
    const grassGeo = this._mergeGeometries([plane1, plane2]);
    grassGeo.translate(0, 0.15, 0);

    const grassMat = new THREE.MeshLambertMaterial({
      color: 0x384830,
      side: THREE.DoubleSide,
    });
    patchMaterial(grassMat);

    const count = 400;
    const instanced = new THREE.InstancedMesh(grassGeo, grassMat, count);
    instanced.name = 'groundScatter-grass';

    this._placeInstances(instanced, count, pathSamples, treeColliders, 8, (dummy, x, z) => {
      dummy.position.set(x, 0, z);
      dummy.rotation.set(0, Math.random() * Math.PI, 0);
      const s = randomRange(0.5, 1.2);
      dummy.scale.set(s, s * randomRange(0.8, 1.5), s);
    });

    this.scene.add(instanced);
    this.instancedMeshes.push(instanced);
  }

  _buildLeaves(pathSamples, treeColliders) {
    const leafGeo = new THREE.PlaneGeometry(0.12, 0.08);
    leafGeo.rotateX(-Math.PI / 2);
    leafGeo.translate(0, 0.015, 0);

    const leafMat = new THREE.MeshLambertMaterial({
      color: 0xffffff,
      side: THREE.DoubleSide,
    });
    patchMaterial(leafMat);

    const count = 200;
    const instanced = new THREE.InstancedMesh(leafGeo, leafMat, count);
    instanced.name = 'groundScatter-leaves';

    const autumnColors = [
      new THREE.Color(0x8a5a2a),
      new THREE.Color(0x9a6830),
      new THREE.Color(0x7a4820),
      new THREE.Color(0x8a5828),
    ];

    this._placeInstances(instanced, count, pathSamples, treeColliders, 6, (dummy, x, z, i) => {
      dummy.position.set(x, 0.01, z);
      dummy.rotation.set(randomRange(-0.1, 0.1), Math.random() * Math.PI * 2, randomRange(-0.1, 0.1));
      const s = randomRange(0.8, 1.5);
      dummy.scale.set(s, 1, s);

      const c = autumnColors[Math.floor(Math.random() * autumnColors.length)];
      instanced.setColorAt(i, c);
    });

    if (instanced.instanceColor) instanced.instanceColor.needsUpdate = true;
    this.scene.add(instanced);
    this.instancedMeshes.push(instanced);
  }

  _buildRocks(pathSamples, treeColliders) {
    const rockGeo = new THREE.DodecahedronGeometry(0.15, 0);
    const rockMat = new THREE.MeshLambertMaterial({ color: 0x6a6a6a });
    patchMaterial(rockMat);

    const count = 100;
    const instanced = new THREE.InstancedMesh(rockGeo, rockMat, count);
    instanced.name = 'groundScatter-rocks';
    instanced.castShadow = true;
    instanced.receiveShadow = true;

    this._placeInstances(instanced, count, pathSamples, treeColliders, 10, (dummy, x, z) => {
      dummy.position.set(x, randomRange(-0.02, 0.05), z);
      dummy.rotation.set(Math.random() * Math.PI, Math.random() * Math.PI, Math.random() * Math.PI);
      const s = randomRange(0.5, 1.5);
      dummy.scale.set(s * randomRange(0.7, 1.3), s * randomRange(0.5, 1.0), s * randomRange(0.7, 1.3));
    });

    this.scene.add(instanced);
    this.instancedMeshes.push(instanced);
  }

  _mergeGeometries(geometries) {
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
    if (indices.length > 0) merged.setIndex(indices);
    return merged;
  }

  dispose() {
    for (const m of this.instancedMeshes) {
      m.geometry.dispose();
      m.material.dispose();
      this.scene.remove(m);
    }
    this.instancedMeshes = [];
  }
}
