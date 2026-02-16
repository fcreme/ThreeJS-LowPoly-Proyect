import * as THREE from 'three';
import Engine from './core/Engine.js';
import EventBus from './core/EventBus.js';
import InputManager from './core/InputManager.js';
import PlayerController from './player/PlayerController.js';
import InteractionRaycaster from './player/InteractionRaycaster.js';
import WorldManager from './world/WorldManager.js';
import ArtistStationLoader from './world/ArtistStationLoader.js';
import ParticleSystem from './fx/ParticleSystem.js';
import Fireflies from './fx/Fireflies.js';
import AudioSystem from './fx/AudioSystem.js';
import UIOverlay from './ui/UIOverlay.js';
import LoadingScreen from './ui/LoadingScreen.js';

class App {
  constructor() {
    this.engine = null;
    this.input = null;
    this.player = null;
    this.interaction = null;
    this.world = null;
    this.stationLoader = null;
    this.particles = null;
    this.fireflies = null;
    this.audio = null;
    this.ui = null;
    this.loading = null;

    this._activeBook = null;
    this._bookCameraTarget = new THREE.Vector3();
    this._bookCameraOriginal = new THREE.Vector3();
    this._bookCameraOriginalQuat = new THREE.Quaternion();
    this._bookCameraTargetQuat = new THREE.Quaternion();
    this._isInBookMode = false;
    this._cameraTransitionProgress = 0;
    this._cameraTransitioning = false;
    this._readingLight = null;
  }

  async init() {
    this.loading = new LoadingScreen();
    this.loading.setProgress(0.1, 'Initializing engine...');

    // Core
    this.engine = new Engine();
    this.input = new InputManager();

    this.loading.setProgress(0.2, 'Building world...');

    // Player
    this.player = new PlayerController(
      this.engine.camera,
      this.engine.renderer.domElement,
      this.engine.scene
    );

    // Place player at path start
    this.engine.camera.position.set(0, 1.6, 38);

    // World
    this.world = new WorldManager(this.engine.scene);
    const { pathCurve, colliders } = await this.world.build();
    this.player.setColliders(colliders);

    this.loading.setProgress(0.5, 'Populating forest...');

    // Particles
    this.particles = new ParticleSystem(this.engine.scene);
    this.particles.build(this.engine.camera.position);

    // Fireflies
    this.fireflies = new Fireflies(this.engine.scene);
    this.fireflies.build(pathCurve);

    // Audio
    this.audio = new AudioSystem(this.engine.camera);
    await this.audio.init();

    this.loading.setProgress(0.6, 'Loading artist stations...');

    // Artist Stations
    this.stationLoader = new ArtistStationLoader(
      this.world.worldGroup,
      pathCurve
    );
    await this.stationLoader.loadManifest();

    // Interaction
    this.interaction = new InteractionRaycaster(this.engine.camera);
    this.interaction.setInteractables(this.stationLoader.getInteractableObjects());

    this.loading.setProgress(0.8, 'Setting up interface...');

    // UI
    this.ui = new UIOverlay();

    // Setup event handlers
    this._setupEvents();

    this.loading.setProgress(1.0, 'Ready');

    // Click to start
    const clickToStart = document.getElementById('click-to-start');
    clickToStart.addEventListener('click', () => {
      this.player.lock();
      this.audio.startAmbient();
      this.loading.hide();
    });

    // Start game loop
    this.engine.start((delta, elapsed) => this._update(delta, elapsed));

    this.loading.hide();
  }

  _setupEvents() {
    // Book interaction via [E] key
    EventBus.on('input:keydown', (code) => {
      if (code === 'KeyE') {
        this._handleInteract();
      }
      if (code === 'KeyQ') {
        this._handleCloseBook();
      }
      if (this._isInBookMode && this._activeBook) {
        if (code === 'KeyD') {
          this._activeBook.nextPage();
        }
        if (code === 'KeyA') {
          this._activeBook.prevPage();
        }
      }
    });

    // Update interaction prompt when book state changes
    EventBus.on('book:closed', () => {
      this._exitBookMode();
    });
  }

  _handleInteract() {
    if (this._isInBookMode) return;

    const target = this.interaction.currentTarget;
    if (!target) return;

    if (target.userData.type === 'book') {
      const station = this.stationLoader.findStationBySlug(target.userData.stationSlug);
      if (station && station.book) {
        this._enterBookMode(station.book);
      }
    }
  }

  _enterBookMode(book) {
    this._activeBook = book;
    this._isInBookMode = true;

    // Save camera state
    this._bookCameraOriginal.copy(this.engine.camera.position);
    this._bookCameraOriginalQuat.copy(this.engine.camera.quaternion);

    // Disable mouse look so it doesn't fight the transition (keep pointer locked)
    this.player.controls.enabled = false;

    // Compute reading position relative to player's approach direction
    const bookWorldPos = new THREE.Vector3();
    book.group.getWorldPosition(bookWorldPos);

    const dir = new THREE.Vector3()
      .subVectors(this.engine.camera.position, bookWorldPos);
    dir.y = 0;
    dir.normalize();

    // Position camera above the book — far enough to see pages comfortably
    this._bookCameraTarget.set(
      bookWorldPos.x + dir.x * 0.55,
      bookWorldPos.y + 0.55,
      bookWorldPos.z + dir.z * 0.55
    );

    // Pre-compute the target quaternion (looking down at the book)
    const tempCam = this.engine.camera.clone();
    tempCam.position.copy(this._bookCameraTarget);
    tempCam.lookAt(bookWorldPos);
    this._bookCameraTargetQuat.copy(tempCam.quaternion);

    this._cameraTransitionProgress = 0;
    this._cameraTransitioning = true;

    // Reading light — warm point light above the book
    this._readingLight = new THREE.PointLight(0xffeedd, 3, 5);
    this._readingLight.position.set(
      bookWorldPos.x + dir.x * 0.3,
      bookWorldPos.y + 0.8,
      bookWorldPos.z + dir.z * 0.3
    );
    this.engine.scene.add(this._readingLight);

    // Lock player movement (WASD)
    this.player.lockMovement();

    // Open the book
    book.open();
  }

  _handleCloseBook() {
    if (!this._isInBookMode || !this._activeBook) return;
    this._activeBook.close();
  }

  _exitBookMode() {
    this._isInBookMode = false;
    this._cameraTransitionProgress = 0;
    this._cameraTransitioning = true;
    this._activeBook = null;

    // Remove reading light
    if (this._readingLight) {
      this.engine.scene.remove(this._readingLight);
      this._readingLight.dispose();
      this._readingLight = null;
    }
  }

  _updateCameraTransition(delta) {
    if (!this._cameraTransitioning) return;

    // Slower, cinematic speed — ~0.8s transition
    const speed = 1.2;
    this._cameraTransitionProgress += delta * speed;

    if (this._cameraTransitionProgress >= 1) {
      this._cameraTransitionProgress = 1;
      this._cameraTransitioning = false;

      if (!this._isInBookMode) {
        // Finished returning — restore exact original state and re-enable controls
        this.engine.camera.position.copy(this._bookCameraOriginal);
        this.engine.camera.quaternion.copy(this._bookCameraOriginalQuat);
        this.player.unlockMovement();
        this.player.controls.enabled = true;
      }
      return;
    }

    const t = this._easeInOutCubic(this._cameraTransitionProgress);

    if (this._isInBookMode) {
      // Moving toward book — lerp position + slerp rotation
      this.engine.camera.position.lerpVectors(
        this._bookCameraOriginal,
        this._bookCameraTarget,
        t
      );
      this.engine.camera.quaternion.slerpQuaternions(
        this._bookCameraOriginalQuat,
        this._bookCameraTargetQuat,
        t
      );
    } else {
      // Returning to original position — reverse lerp + slerp
      this.engine.camera.position.lerpVectors(
        this._bookCameraTarget,
        this._bookCameraOriginal,
        t
      );
      this.engine.camera.quaternion.slerpQuaternions(
        this._bookCameraTargetQuat,
        this._bookCameraOriginalQuat,
        t
      );
    }
  }

  _easeInOutCubic(t) {
    return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
  }

  _update(delta, elapsed) {
    // Player
    if (!this._isInBookMode) {
      this.player.update(delta);
    }

    // Camera transition
    this._updateCameraTransition(delta);

    // Interaction raycaster (only when not in book mode)
    if (!this._isInBookMode && this.player.isLocked) {
      this.interaction.update();
    }

    // Station proximity loading
    this.stationLoader.update(this.engine.camera.position);

    // Update loaded stations (book animations, lights)
    for (const station of this.stationLoader.getStations()) {
      station.update(delta, elapsed);
    }

    // Particles
    this.particles.update(delta, elapsed, this.engine.camera.position);

    // Fireflies
    this.fireflies.update(delta, elapsed);

    // Audio
    this.audio.update(delta);
  }
}

// Boot
const app = new App();
app.init().catch(console.error);
