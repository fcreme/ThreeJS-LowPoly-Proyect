import * as THREE from 'three';

export default class SkyDome {
  constructor() {
    this.group = new THREE.Group();
    this.group.name = 'skyDome';
  }

  build() {
    this._buildSky();
    this._buildStars();
    this._buildMoon();
    return this.group;
  }

  _buildSky() {
    const radius = 190;
    const geo = new THREE.SphereGeometry(radius, 32, 16);
    const posAttr = geo.attributes.position;
    const colors = new Float32Array(posAttr.count * 3);

    for (let i = 0; i < posAttr.count; i++) {
      const y = posAttr.getY(i);
      const t = Math.max(0, (y / radius + 1) / 2);

      // Horizon: dark warm brown-grey matching fog 0x0e0c0a
      // Zenith: muted dark blue
      colors[i * 3] = (14 + t * 6) / 255;
      colors[i * 3 + 1] = (12 + t * 10) / 255;
      colors[i * 3 + 2] = (10 + t * 25) / 255;
    }

    geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));

    const mat = new THREE.MeshBasicMaterial({
      vertexColors: true,
      side: THREE.BackSide,
      fog: false,
      depthWrite: false,
    });

    const mesh = new THREE.Mesh(geo, mat);
    mesh.renderOrder = -1000;
    this.group.add(mesh);
  }

  _buildStars() {
    const count = 150;
    const positions = new Float32Array(count * 3);

    for (let i = 0; i < count; i++) {
      const theta = Math.random() * Math.PI * 2;
      const phi = Math.random() * Math.PI * 0.4;
      const r = 185;

      positions[i * 3] = r * Math.sin(phi) * Math.cos(theta);
      positions[i * 3 + 1] = r * Math.cos(phi);
      positions[i * 3 + 2] = r * Math.sin(phi) * Math.sin(theta);
    }

    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));

    const mat = new THREE.PointsMaterial({
      color: 0xaaaacc,
      size: 0.4,
      sizeAttenuation: true,
      transparent: true,
      opacity: 0.4,
      fog: false,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
    });

    const stars = new THREE.Points(geo, mat);
    stars.renderOrder = -999;
    this.group.add(stars);
  }

  _buildMoon() {
    const moonDir = new THREE.Vector3(-30, 40, -20).normalize();
    const geo = new THREE.CircleGeometry(4, 12);
    const mat = new THREE.MeshBasicMaterial({
      color: 0xccddee,
      fog: false,
      depthWrite: false,
      transparent: true,
      opacity: 0.6,
      side: THREE.DoubleSide,
    });

    const moon = new THREE.Mesh(geo, mat);
    moon.position.copy(moonDir.multiplyScalar(180));
    moon.lookAt(0, 0, 0);
    moon.renderOrder = -998;
    this.group.add(moon);
  }

  dispose() {
    this.group.traverse((child) => {
      if (child.geometry) child.geometry.dispose();
      if (child.material) child.material.dispose();
    });
  }
}
