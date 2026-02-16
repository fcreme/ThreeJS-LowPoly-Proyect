import * as THREE from 'three';
import { PointerLockControls } from 'three/addons/controls/PointerLockControls.js';
import EventBus from '../core/EventBus.js';

export default class PlayerController {
  constructor(camera, domElement, scene) {
    this.camera = camera;
    this.enabled = true;
    this._movementLocked = false;

    // PointerLock
    this.controls = new PointerLockControls(camera, domElement);
    scene.add(this.controls.object);

    // Movement
    this._velocity = new THREE.Vector3();
    this._direction = new THREE.Vector3();
    this._moveSpeed = 1.6;
    this._friction = 12.0;
    this._keys = { forward: false, backward: false, left: false, right: false };

    // Bobbing
    this._bobTime = 0;
    this._bobAmount = 0.04;
    this._bobSpeed = 8;
    this._baseY = 1.6;

    // Collision — stored as {x, z, radius} for fast distance checks (no raycasting)
    this._treePositions = [];

    // Input events
    this._unsubKeyDown = EventBus.on('input:keydown', (code) => this._onKeyDown(code));
    this._unsubKeyUp = EventBus.on('input:keyup', (code) => this._onKeyUp(code));

    // Lock/unlock events
    this.controls.addEventListener('lock', () => {
      EventBus.emit('player:locked');
    });
    this.controls.addEventListener('unlock', () => {
      EventBus.emit('player:unlocked');
    });
  }

  get isLocked() {
    return this.controls.isLocked;
  }

  lock() {
    this.controls.lock();
  }

  lockMovement() {
    this._movementLocked = true;
    this._keys.forward = false;
    this._keys.backward = false;
    this._keys.left = false;
    this._keys.right = false;
  }

  unlockMovement() {
    this._movementLocked = false;
  }

  setColliders(positions) {
    // positions: array of {x, z, radius}
    this._treePositions = positions;
  }

  _onKeyDown(code) {
    if (this._movementLocked) return;
    switch (code) {
      case 'KeyW': this._keys.forward = true; break;
      case 'KeyS': this._keys.backward = true; break;
      case 'KeyA': this._keys.left = true; break;
      case 'KeyD': this._keys.right = true; break;
    }
  }

  _onKeyUp(code) {
    switch (code) {
      case 'KeyW': this._keys.forward = false; break;
      case 'KeyS': this._keys.backward = false; break;
      case 'KeyA': this._keys.left = false; break;
      case 'KeyD': this._keys.right = false; break;
    }
  }

  update(delta) {
    if (!this.controls.isLocked || !this.enabled) return;

    // Direction from keys
    this._direction.set(0, 0, 0);
    if (this._keys.forward) this._direction.z -= 1;
    if (this._keys.backward) this._direction.z += 1;
    if (this._keys.left) this._direction.x -= 1;
    if (this._keys.right) this._direction.x += 1;
    this._direction.normalize();

    // Apply movement in camera-relative direction
    if (this._direction.lengthSq() > 0) {
      const forward = new THREE.Vector3();
      this.camera.getWorldDirection(forward);
      forward.y = 0;
      forward.normalize();

      const right = new THREE.Vector3();
      right.crossVectors(forward, new THREE.Vector3(0, 1, 0)).normalize();

      this._velocity.addScaledVector(forward, -this._direction.z * this._moveSpeed * delta);
      this._velocity.addScaledVector(right, this._direction.x * this._moveSpeed * delta);
    }

    // Friction
    this._velocity.multiplyScalar(Math.max(0, 1 - this._friction * delta));

    // Simple distance-based collision (no raycasting — just check XZ distance to trees)
    const nextX = this.camera.position.x + this._velocity.x;
    const nextZ = this.camera.position.z + this._velocity.z;
    for (let i = 0; i < this._treePositions.length; i++) {
      const tree = this._treePositions[i];
      const dx = nextX - tree.x;
      const dz = nextZ - tree.z;
      if (dx * dx + dz * dz < tree.radius * tree.radius) {
        this._velocity.set(0, 0, 0);
        break;
      }
    }

    // Apply position
    this.camera.position.x += this._velocity.x;
    this.camera.position.z += this._velocity.z;

    // Keep within world bounds
    this.camera.position.x = THREE.MathUtils.clamp(this.camera.position.x, -95, 95);
    this.camera.position.z = THREE.MathUtils.clamp(this.camera.position.z, -95, 95);

    // Head bobbing
    const isMoving = this._direction.lengthSq() > 0;
    if (isMoving) {
      this._bobTime += delta * this._bobSpeed;
      this.camera.position.y = this._baseY + Math.sin(this._bobTime) * this._bobAmount;
    } else {
      // Smoothly return to base height
      this.camera.position.y += (this._baseY - this.camera.position.y) * 5 * delta;
    }

    EventBus.emit('player:moved', {
      position: this.camera.position,
      direction: this.camera.getWorldDirection(new THREE.Vector3()),
    });
  }

  dispose() {
    this._unsubKeyDown();
    this._unsubKeyUp();
    this.controls.dispose();
  }
}
