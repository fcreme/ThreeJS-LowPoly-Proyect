import * as THREE from 'three';
import { loadFBX } from './ModelLoader.js';

const CORRIDOR_PIECES = [
  'hotel_hallway_01',
  'hotel_hallway_doors_01',
  'hotel_hallway_doors_02',
  'hotel_hallway_arch_01',
];

export default class HotelHallway {
  constructor(scene) {
    this.scene = scene;
    this.group = new THREE.Group();
    this.group.name = 'hotelHallway';
    this.scene.add(this.group);
    this._tiles = [];
    this._lights = [];
  }

  async build(pathCurve) {
    const fullModel = await loadFBX('/models/hotel/hotel_hallway3.fbx');

    // Extract corridor pieces
    const allPieces = {};
    fullModel.traverse((child) => {
      if (!child.isMesh) return;
      if (!CORRIDOR_PIECES.includes(child.name)) return;

      const mesh = child.clone();
      mesh.scale.multiplyScalar(0.01);

      const box = new THREE.Box3().setFromObject(mesh);
      const size = box.getSize(new THREE.Vector3());
      const center = box.getCenter(new THREE.Vector3());
      const min = box.min;

      mesh.position.set(
        mesh.position.x - center.x,
        mesh.position.y - min.y,
        mesh.position.z - center.z
      );

      const template = new THREE.Group();
      template.add(mesh);
      allPieces[child.name] = { template, size };

      console.log(`[HotelHallway] "${child.name}" → ${size.x.toFixed(2)} x ${size.y.toFixed(2)} x ${size.z.toFixed(2)}`);
    });

    const pieceNames = Object.keys(allPieces);
    const ref = allPieces[pieceNames[0]];
    const targetHeight = 4.0;
    const scale = targetHeight / ref.size.y;
    const tileLength = Math.max(ref.size.x, ref.size.z) * scale;

    // Grid: rows = piece types, cols = 4 rotations (0°, 90°, 180°, 270°)
    const rotations = [0, Math.PI / 2, Math.PI, Math.PI * 1.5];
    const rotLabels = ['0°', '90°', '180°', '270°'];
    const colSpacing = tileLength + 4;
    const rowSpacing = tileLength + 4;
    const startX = -((rotations.length - 1) * colSpacing) / 2;
    const startZ = 32;

    for (let ri = 0; ri < pieceNames.length; ri++) {
      const pieceName = pieceNames[ri];
      const piece = allPieces[pieceName];

      // Row label
      const rowLabel = this._makeLabel(pieceName, '#00ff88');
      rowLabel.position.set(startX - 8, 3, startZ - ri * rowSpacing);
      this.group.add(rowLabel);

      for (let ci = 0; ci < rotations.length; ci++) {
        const wrapper = new THREE.Group();
        const tile = piece.template.clone();
        tile.scale.setScalar(scale);
        wrapper.add(tile);

        const x = startX + ci * colSpacing;
        const z = startZ - ri * rowSpacing;
        wrapper.position.set(x, 0, z);
        wrapper.rotation.y = rotations[ci];

        this.group.add(wrapper);
        this._tiles.push(wrapper);

        // Rotation label
        if (ri === 0) {
          const colLabel = this._makeLabel(rotLabels[ci], '#ff8844');
          colLabel.position.set(x, targetHeight + 2, startZ + 4);
          this.group.add(colLabel);
        }

        // Light
        const light = new THREE.PointLight(0xffe0aa, 3.0, 12);
        light.position.set(x, targetHeight * 0.8, z);
        this.group.add(light);
        this._lights.push(light);
      }
    }

    // Fill light
    const sun = new THREE.DirectionalLight(0xffeedd, 1.5);
    sun.position.set(5, 20, 30);
    this.group.add(sun);

    console.log(`[HotelHallway] === ROTATION TEST ===`);
    console.log(`[HotelHallway] Rows: ${pieceNames.join(', ')}`);
    console.log(`[HotelHallway] Columns: 0°, 90°, 180°, 270°`);
    console.log(`[HotelHallway] Tell me which rotation looks correct for each piece!`);
  }

  _makeLabel(text, color = '#ffcc00') {
    const canvas = document.createElement('canvas');
    canvas.width = 1024;
    canvas.height = 128;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#000000cc';
    ctx.fillRect(0, 0, 1024, 128);
    ctx.fillStyle = color;
    ctx.font = 'bold 32px monospace';
    ctx.textAlign = 'center';
    ctx.fillText(text, 512, 75);

    const tex = new THREE.CanvasTexture(canvas);
    tex.minFilter = THREE.NearestFilter;
    tex.magFilter = THREE.NearestFilter;

    const mat = new THREE.SpriteMaterial({ map: tex, depthTest: false });
    const sprite = new THREE.Sprite(mat);
    sprite.scale.set(8, 1, 1);
    return sprite;
  }

  dispose() {
    for (const tile of this._tiles) {
      tile.traverse((child) => {
        if (child.isMesh) {
          child.geometry?.dispose();
          if (child.material) {
            child.material.map?.dispose();
            child.material.dispose();
          }
        }
      });
      this.group.remove(tile);
    }
    for (const light of this._lights) {
      light.dispose?.();
      this.group.remove(light);
    }
    this._tiles = [];
    this._lights = [];
    this.scene.remove(this.group);
  }
}
