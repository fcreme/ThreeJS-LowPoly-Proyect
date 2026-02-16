import * as THREE from 'three';
import TerrainBuilder from './TerrainBuilder.js';
import PathBuilder from './PathBuilder.js';
import ForestGenerator from './ForestGenerator.js';
import SkyDome from './SkyDome.js';
import GroundScatter from './GroundScatter.js';
import HotelHallway from './HotelHallway.js';
import EventBus from '../core/EventBus.js';

export default class WorldManager {
  constructor(scene) {
    this.scene = scene;
    this.worldGroup = new THREE.Group();
    this.worldGroup.name = 'world';
    this.scene.add(this.worldGroup);

    this.terrain = new TerrainBuilder(this.worldGroup);
    this.path = new PathBuilder(this.worldGroup);
    this.forest = new ForestGenerator(this.worldGroup);
    this.skyDome = new SkyDome();
    this.groundScatter = new GroundScatter(this.worldGroup);
    this.hotelHallway = new HotelHallway(this.worldGroup);

    // Lighting
    this._setupLighting();
  }

  _setupLighting() {
    // Ambient — strong warm fill so geometry is always visible
    this.ambient = new THREE.AmbientLight(0x584838, 1.5);
    this.scene.add(this.ambient);

    // Hemisphere — warm sky / cool ground for subtle contrast
    this.hemiLight = new THREE.HemisphereLight(0x887766, 0x2a2020, 1.0);
    this.scene.add(this.hemiLight);

    // Moonlight — directional with shadows, creates visible light/dark contrast
    this.moonlight = new THREE.DirectionalLight(0x8899bb, 1.2);
    this.moonlight.position.set(-30, 40, -20);
    this.moonlight.castShadow = true;
    this.moonlight.shadow.mapSize.width = 1024;
    this.moonlight.shadow.mapSize.height = 1024;
    this.moonlight.shadow.camera.near = 0.5;
    this.moonlight.shadow.camera.far = 100;
    this.moonlight.shadow.camera.left = -50;
    this.moonlight.shadow.camera.right = 50;
    this.moonlight.shadow.camera.top = 50;
    this.moonlight.shadow.camera.bottom = -50;
    this.moonlight.shadow.bias = -0.001;
    this.scene.add(this.moonlight);
    this.scene.add(this.moonlight.target);
  }

  async build() {
    const skyGroup = this.skyDome.build();
    this.scene.add(skyGroup);

    this.terrain.build();
    const { curve } = this.path.build();
    const colliders = this.forest.build(curve);
    this.groundScatter.build(curve, colliders);

    // Load hotel hallway tiles along the path
    await this.hotelHallway.build(curve);

    return { pathCurve: curve, colliders };
  }

  get pathCurve() {
    return this.path.curve;
  }

  dispose() {
    this.skyDome.dispose();
    this.scene.remove(this.skyDome.group);
    this.terrain.dispose();
    this.path.dispose();
    this.forest.dispose();
    this.groundScatter.dispose();
    this.hotelHallway.dispose();
    this.scene.remove(this.ambient);
    this.scene.remove(this.hemiLight);
    this.scene.remove(this.moonlight);
    this.scene.remove(this.worldGroup);
  }
}
