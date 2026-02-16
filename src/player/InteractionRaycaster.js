import * as THREE from 'three';
import EventBus from '../core/EventBus.js';

export default class InteractionRaycaster {
  constructor(camera) {
    this.camera = camera;
    this.raycaster = new THREE.Raycaster();
    this.raycaster.far = 4; // Interaction range
    this._interactables = [];
    this._currentTarget = null;
    this._screenCenter = new THREE.Vector2(0, 0);
  }

  setInteractables(objects) {
    this._interactables = objects;
  }

  addInteractable(obj) {
    this._interactables.push(obj);
  }

  removeInteractable(obj) {
    const idx = this._interactables.indexOf(obj);
    if (idx !== -1) this._interactables.splice(idx, 1);
  }

  update() {
    this.raycaster.setFromCamera(this._screenCenter, this.camera);
    const intersects = this.raycaster.intersectObjects(this._interactables, true);

    let newTarget = null;
    if (intersects.length > 0) {
      // Walk up to find the interactable root
      let obj = intersects[0].object;
      while (obj && !obj.userData.interactable) {
        obj = obj.parent;
      }
      if (obj && obj.userData.interactable) {
        newTarget = obj;
      }
    }

    if (newTarget !== this._currentTarget) {
      if (this._currentTarget) {
        EventBus.emit('interaction:exit', { target: this._currentTarget });
      }
      this._currentTarget = newTarget;
      if (newTarget) {
        EventBus.emit('interaction:enter', { target: newTarget });
      }
    }
  }

  get currentTarget() {
    return this._currentTarget;
  }

  activate() {
    if (this._currentTarget) {
      EventBus.emit('interaction:activate', { target: this._currentTarget });
    }
  }
}
