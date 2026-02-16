import * as THREE from 'three';
import ArtistStation from './ArtistStation.js';
import EventBus from '../core/EventBus.js';

export default class ArtistStationLoader {
  constructor(scene, pathCurve) {
    this.scene = scene;
    this.pathCurve = pathCurve;
    this.stationsGroup = new THREE.Group();
    this.stationsGroup.name = 'stations';
    this.scene.add(this.stationsGroup);

    this._stations = [];
  }

  /**
   * Loads manifest and pre-loads ALL stations upfront (during loading screen).
   * This avoids mid-gameplay freezes from canvas texture compositing.
   */
  async loadManifest() {
    try {
      const res = await fetch('/data/artists/manifest.json');
      const slugs = await res.json();

      for (const slug of slugs) {
        const configRes = await fetch(`/data/artists/${slug}/config.json`);
        const config = await configRes.json();
        config.slug = slug;

        const station = new ArtistStation(config, this.pathCurve);
        this.stationsGroup.add(station.group);
        this._stations.push(station);

        // Pre-load station now (while loading screen is visible)
        await station.load();
        EventBus.emit('station:loaded', { slug: station.slug });
      }
    } catch (e) {
      console.warn('Could not load artist manifest:', e);
    }
  }

  update(playerPosition) {
    // Stations are pre-loaded — nothing to do here for loading/unloading
  }

  getStations() {
    return this._stations;
  }

  getLoadedStations() {
    return this._stations.filter(s => s.loaded);
  }

  getInteractableObjects() {
    const objects = [];
    for (const station of this._stations) {
      if (station.loaded && station.book) {
        objects.push(station.book.group);
      }
    }
    return objects;
  }

  findStationBySlug(slug) {
    return this._stations.find(s => s.slug === slug);
  }

  dispose() {
    for (const station of this._stations) {
      station.dispose();
    }
    this.scene.remove(this.stationsGroup);
  }
}
