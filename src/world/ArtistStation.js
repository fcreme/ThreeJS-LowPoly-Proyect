import * as THREE from 'three';
import FlickerLight from '../fx/FlickerLight.js';
import BookFactory from '../book/BookFactory.js';
import EventBus from '../core/EventBus.js';
import { patchMaterial } from '../fx/PSXEffect.js';

export default class ArtistStation {
  constructor(config, pathCurve) {
    this.config = config;
    this.slug = config.slug;
    this.group = new THREE.Group();
    this.group.name = `station-${config.slug}`;
    this.loaded = false;

    this.book = null;
    this.flickerLight = null;
    this._props = [];
    this._loading = false;

    // Position along path
    const pathT = config.pathT || 0.3;
    const side = config.side || 'right';
    const pos = pathCurve.getPointAt(pathT);
    const tangent = pathCurve.getTangentAt(pathT);
    const normal = new THREE.Vector3().crossVectors(tangent, new THREE.Vector3(0, 1, 0)).normalize();

    const offset = side === 'right' ? 3.5 : -3.5;
    this.group.position.set(
      pos.x + normal.x * offset,
      0,
      pos.z + normal.z * offset
    );

    // Face toward the path
    this.group.lookAt(pos.x, 0, pos.z);
  }

  async load() {
    if (this.loaded || this._loading) return;
    this._loading = true;

    const factory = new BookFactory();
    this.book = await factory.create(this.config);

    // Position book on the altar slab
    this.book.group.position.set(0, 0.78, 0);
    this.group.add(this.book.group);

    // Mark book group as interactable
    this.book.group.userData.interactable = true;
    this.book.group.userData.type = 'book';
    this.book.group.userData.stationSlug = this.slug;
    this.book.group.userData.promptText = `[E] Open Book — ${this.config.title || 'Unknown Artist'}`;

    // Stone altar
    const altarMat = new THREE.MeshLambertMaterial({ color: 0x4a4038 });

    // Base cylinder
    const baseGeo = new THREE.CylinderGeometry(0.35, 0.4, 0.7, 8);
    const base = new THREE.Mesh(baseGeo, altarMat);
    base.position.set(0, 0.35, 0);
    base.castShadow = true;
    base.receiveShadow = true;
    this.group.add(base);
    this._props.push(base);

    // Slab on top
    const slabGeo = new THREE.BoxGeometry(0.55, 0.06, 0.45);
    const slab = new THREE.Mesh(slabGeo, altarMat);
    slab.position.set(0, 0.73, 0);
    slab.castShadow = true;
    slab.receiveShadow = true;
    this.group.add(slab);
    this._props.push(slab);

    // Plinth at bottom
    const plinthGeo = new THREE.CylinderGeometry(0.42, 0.45, 0.1, 8);
    const plinth = new THREE.Mesh(plinthGeo, altarMat);
    plinth.position.set(0, 0.05, 0);
    plinth.castShadow = true;
    plinth.receiveShadow = true;
    this.group.add(plinth);
    this._props.push(plinth);

    // Flickering warm light
    const lightPos = this.group.position.clone();
    lightPos.y = 2.5;
    this.flickerLight = new FlickerLight(
      this.group,
      new THREE.Vector3(0, 2.5, 0),
      0xff9955,
      3.0,
      18
    );

    // Decorative props
    this._buildProps();

    // PSX vertex jitter on all station meshes
    this.group.traverse((child) => {
      if (child.isMesh && child.material && child.material.isMeshLambertMaterial) {
        patchMaterial(child.material);
      }
    });

    this.loaded = true;
  }

  _buildProps() {
    // --- Tattered banner behind the book ---
    const bannerGeo = new THREE.PlaneGeometry(0.8, 1.2, 4, 8);
    // Vertex displacement for tattered look
    const bannerPos = bannerGeo.attributes.position;
    for (let i = 0; i < bannerPos.count; i++) {
      const y = bannerPos.getY(i);
      const normalizedY = (y + 0.6) / 1.2; // 0 at top, 1 at bottom
      const wave = Math.sin(bannerPos.getX(i) * 4 + y * 3) * 0.06 * normalizedY;
      bannerPos.setZ(i, bannerPos.getZ(i) + wave);
    }
    bannerGeo.computeVertexNormals();
    const bannerMat = new THREE.MeshLambertMaterial({
      color: 0x5a4028,
      side: THREE.DoubleSide,
      transparent: true,
      opacity: 0.85,
    });
    const banner = new THREE.Mesh(bannerGeo, bannerMat);
    banner.position.set(0, 2, -0.3);
    banner.castShadow = true;
    this.group.add(banner);
    this._props.push(banner);

    // --- Ground stones ---
    for (let i = 0; i < 5; i++) {
      const stoneGeo = new THREE.SphereGeometry(0.05 + Math.random() * 0.08, 5, 4);
      const stoneMat = new THREE.MeshLambertMaterial({ color: 0x5a5a5a });
      const stone = new THREE.Mesh(stoneGeo, stoneMat);
      stone.position.set(
        (Math.random() - 0.5) * 1.5,
        0.03,
        (Math.random() - 0.5) * 1.5
      );
      stone.castShadow = true;
      this.group.add(stone);
      this._props.push(stone);
    }

    // --- 3 candles at varying heights around the book ---
    const candleDefs = [
      { h: 0.15, x: 0.18, z: 0.12 },
      { h: 0.10, x: -0.15, z: 0.10 },
      { h: 0.12, x: 0.05, z: -0.16 },
    ];
    const candleMat = new THREE.MeshLambertMaterial({
      color: 0xddc9a0,
      emissive: 0x1a1000,
      emissiveIntensity: 0.3,
    });
    const flameMat = new THREE.MeshLambertMaterial({
      color: 0xff8844,
      emissive: 0xff6622,
      emissiveIntensity: 4.0,
      transparent: true,
      opacity: 0.9,
    });

    for (const def of candleDefs) {
      // Candle body
      const cGeo = new THREE.CylinderGeometry(0.02, 0.025, def.h, 6);
      const candle = new THREE.Mesh(cGeo, candleMat);
      candle.position.set(def.x, 0.76 + def.h / 2, def.z);
      candle.castShadow = true;
      this.group.add(candle);
      this._props.push(candle);

      // Flame — elongated sphere for bloom pickup
      const fGeo = new THREE.SphereGeometry(0.015, 6, 6);
      const flame = new THREE.Mesh(fGeo, flameMat);
      flame.scale.set(1, 2.5, 1);
      flame.position.set(def.x, 0.76 + def.h + 0.02, def.z);
      this.group.add(flame);
      this._props.push(flame);
    }

    // --- Twigs on ground ---
    const twigMat = new THREE.MeshLambertMaterial({ color: 0x5a4828 });
    for (let i = 0; i < 3; i++) {
      const twigGeo = new THREE.CylinderGeometry(0.008, 0.012, 0.4, 4);
      const twig = new THREE.Mesh(twigGeo, twigMat);
      twig.position.set(
        (Math.random() - 0.5) * 1.2,
        0.02,
        (Math.random() - 0.5) * 1.2
      );
      twig.rotation.z = Math.random() * 0.6 - 0.3;
      twig.rotation.x = Math.random() * 0.4 - 0.2;
      this.group.add(twig);
      this._props.push(twig);
    }

    // --- Dark reflective orb at pedestal base ---
    const orbGeo = new THREE.SphereGeometry(0.08, 12, 10);
    const orbMat = new THREE.MeshLambertMaterial({ color: 0x303030 });
    const orb = new THREE.Mesh(orbGeo, orbMat);
    orb.position.set(0.2, 0.08, 0.2);
    this.group.add(orb);
    this._props.push(orb);
  }

  update(delta, elapsed) {
    if (!this.loaded) return;

    if (this.book) {
      this.book.update(delta, elapsed);
    }

    if (this.flickerLight) {
      this.flickerLight.update(elapsed);
    }
  }

  unload() {
    if (this.book) {
      this.book.dispose();
      this.book = null;
    }
    for (const prop of this._props) {
      prop.geometry.dispose();
      prop.material.dispose();
      this.group.remove(prop);
    }
    this._props = [];
    if (this.flickerLight) {
      this.flickerLight.dispose(this.group);
      this.flickerLight = null;
    }
    this.loaded = false;
  }

  dispose() {
    this.unload();
  }
}
