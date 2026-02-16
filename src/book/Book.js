import * as THREE from 'three';
import EventBus from '../core/EventBus.js';
import PageGeometryBuilder from './PageGeometryBuilder.js';
import PageCurlAnimator from './PageCurlAnimator.js';
import PageMaterial from './PageMaterial.js';
import { easeInOutCubic } from '../utils/math.js';

// States
const CLOSED = 'CLOSED';
const OPENING = 'OPENING';
const OPEN = 'OPEN';
const TURNING = 'TURNING';
const CLOSING = 'CLOSING';

export default class Book {
  /**
   * @param {Object} config - Artist config with pages, cover texture, etc.
   * @param {THREE.Texture[]} pageTextures - Composited page textures (front/back pairs)
   * @param {THREE.Texture} coverTexture - Cover texture
   */
  constructor(config, pageTextures, coverTexture) {
    this.config = config;
    this.state = CLOSED;
    this.group = new THREE.Group();
    this.group.name = `book-${config.slug || 'default'}`;

    this._pageTextures = pageTextures;
    this._coverTexture = coverTexture;
    this._currentPage = 0;
    this._totalPages = Math.floor(pageTextures.length / 2);
    this._turnProgress = 0;
    this._turnDirection = 1;
    this._turnSpeed = 1.2;
    this._openProgress = 0;
    this._openSpeed = 1.5;

    this._pages = [];
    this._animators = [];
    this._coverFront = null;
    this._coverBack = null;
    this._spine = null;
    this._hitbox = null;

    this._build();
  }

  _build() {
    const coverWidth = 0.22;
    const coverHeight = 0.30;
    const coverDepth = 0.008;
    const spineWidth = 0.02 + this._totalPages * 0.002;

    // Spine
    const spineGeo = new THREE.BoxGeometry(spineWidth, coverHeight, coverDepth);
    const spineMat = new THREE.MeshStandardMaterial({
      color: 0x2a1a0a,
      roughness: 0.8,
      metalness: 0.1,
    });
    this._spine = new THREE.Mesh(spineGeo, spineMat);
    this._spine.position.set(0, 0, 0);
    this._spine.castShadow = true;
    this.group.add(this._spine);

    // Front cover (opens to the left)
    const coverGeo = new THREE.BoxGeometry(coverWidth, coverHeight, coverDepth);
    const coverMat = new THREE.MeshStandardMaterial({
      color: 0x3a1a0a,
      roughness: 0.7,
      metalness: 0.1,
      map: this._coverTexture || null,
    });

    this._coverFront = new THREE.Mesh(coverGeo, coverMat);
    this._coverFront.position.set(coverWidth / 2 + spineWidth / 2, 0, coverDepth / 2);
    this._coverFront.castShadow = true;
    // Pivot point at spine edge
    this._coverFrontPivot = new THREE.Group();
    this._coverFrontPivot.position.set(spineWidth / 2, 0, coverDepth / 2);
    this._coverFront.position.set(coverWidth / 2, 0, 0);
    this._coverFrontPivot.add(this._coverFront);
    this.group.add(this._coverFrontPivot);

    // Back cover (stays flat)
    const coverBackMat = new THREE.MeshStandardMaterial({
      color: 0x3a1a0a,
      roughness: 0.7,
      metalness: 0.1,
    });
    this._coverBack = new THREE.Mesh(coverGeo.clone(), coverBackMat);
    this._coverBack.position.set(-coverWidth / 2 - spineWidth / 2, 0, -coverDepth / 2);
    this._coverBack.castShadow = true;
    this.group.add(this._coverBack);

    // Build pages
    const pageBuilder = new PageGeometryBuilder();

    for (let i = 0; i < this._totalPages; i++) {
      const frontIdx = i * 2;
      const backIdx = i * 2 + 1;

      const frontTex = this._pageTextures[frontIdx] || this._createBlankTexture();
      const backTex = this._pageTextures[backIdx] || this._createBlankTexture();

      const material = PageMaterial.create(frontTex, backTex);
      const { mesh, bones, skeleton } = pageBuilder.build(material);

      // Position page — stack them with slight Z offset
      const pageGroup = new THREE.Group();
      pageGroup.position.set(spineWidth / 2, 0, coverDepth * 0.5 - i * 0.001);
      pageGroup.add(mesh);
      this.group.add(pageGroup);

      const animator = new PageCurlAnimator(bones);

      this._pages.push({ mesh, bones, skeleton, group: pageGroup, material });
      this._animators.push(animator);
    }

    // Invisible hitbox for cheap raycasting (instead of testing SkinnedMesh pages)
    const hitboxGeo = new THREE.BoxGeometry(
      coverWidth * 2 + spineWidth, coverHeight, coverDepth + 0.02
    );
    const hitboxMat = new THREE.MeshBasicMaterial({ visible: false });
    this._hitbox = new THREE.Mesh(hitboxGeo, hitboxMat);
    this._hitbox.position.set(0, 0, 0);
    this.group.add(this._hitbox);

    // Book rests flat on its back by default (spine horizontal)
    this.group.rotation.x = -Math.PI / 2;
  }

  _createBlankTexture() {
    const canvas = document.createElement('canvas');
    canvas.width = 256;
    canvas.height = 256;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#d4c5a9';
    ctx.fillRect(0, 0, 256, 256);
    const tex = new THREE.CanvasTexture(canvas);
    tex.colorSpace = THREE.SRGBColorSpace;
    return tex;
  }

  // --- State machine ---

  open() {
    if (this.state !== CLOSED) return;
    this.state = OPENING;
    this._openProgress = 0;
    this._currentPage = 0;
    EventBus.emit('book:opening', { slug: this.config.slug });
  }

  close() {
    if (this.state !== OPEN) return;
    this.state = CLOSING;
    this._openProgress = 1;
    EventBus.emit('book:closing', { slug: this.config.slug });
  }

  nextPage() {
    if (this.state !== OPEN) return;
    if (this._currentPage >= this._totalPages - 1) return;
    this.state = TURNING;
    this._turnProgress = 0;
    this._turnDirection = 1;
    EventBus.emit('book:pageTurn', { slug: this.config.slug, page: this._currentPage, direction: 'next' });
  }

  prevPage() {
    if (this.state !== OPEN) return;
    if (this._currentPage <= 0) return;
    this._currentPage--;
    this.state = TURNING;
    this._turnProgress = 1;
    this._turnDirection = -1;
    EventBus.emit('book:pageTurn', { slug: this.config.slug, page: this._currentPage, direction: 'prev' });
  }

  update(delta, elapsed) {
    switch (this.state) {
      case OPENING:
        this._updateOpening(delta);
        break;
      case CLOSING:
        this._updateClosing(delta);
        break;
      case TURNING:
        this._updateTurning(delta, elapsed);
        break;
      case OPEN:
        // Idle — subtle micro-animations
        this._updateIdle(elapsed);
        break;
    }
    this._updatePageVisibility();
  }

  _updatePageVisibility() {
    const cp = this._currentPage;
    for (let i = 0; i < this._pages.length; i++) {
      // Only show the current page, the previous turned page, and the next page
      // (at most 3 visible at once). Hide everything else to prevent z-fighting.
      this._pages[i].group.visible =
        this.state !== CLOSED && i >= cp - 1 && i <= cp + 1;
    }
  }

  _updateOpening(delta) {
    this._openProgress += delta * this._openSpeed;

    if (this._openProgress >= 1) {
      this._openProgress = 1;
      this.state = OPEN;
      EventBus.emit('book:open', { slug: this.config.slug });
    }

    const easedOpen = easeInOutCubic(this._openProgress);
    // Rotate front cover open (from 0 to -PI)
    this._coverFrontPivot.rotation.y = -easedOpen * Math.PI;

    // Tilt book upright so pages face the reader
    this.group.rotation.x = -Math.PI / 2 + easedOpen * (Math.PI / 3);
  }

  _updateClosing(delta) {
    this._openProgress -= delta * this._openSpeed;

    if (this._openProgress <= 0) {
      this._openProgress = 0;
      this.state = CLOSED;
      // Reset all pages
      for (const animator of this._animators) {
        animator.reset();
      }
      this._currentPage = 0;
      EventBus.emit('book:closed', { slug: this.config.slug });
    }

    const easedOpen = easeInOutCubic(this._openProgress);
    this._coverFrontPivot.rotation.y = -easedOpen * Math.PI;
    this.group.rotation.x = -Math.PI / 2 + easedOpen * (Math.PI / 3);
  }

  _updateTurning(delta, elapsed) {
    this._turnProgress += this._turnDirection * delta * this._turnSpeed;

    const animator = this._animators[this._currentPage];
    if (animator) {
      animator.update(this._turnProgress, elapsed);
    }

    if (this._turnDirection > 0 && this._turnProgress >= 1) {
      // Forward turn complete
      if (animator) animator.setFullyTurned();
      this._currentPage++;
      this.state = OPEN;
    } else if (this._turnDirection < 0 && this._turnProgress <= 0) {
      // Backward turn complete
      if (animator) animator.reset();
      this.state = OPEN;
    }
  }

  _updateIdle(elapsed) {
    // Subtle breathing motion on current visible page
    // Nothing for now — can add micro-sway later
  }

  get currentPageIndex() {
    return this._currentPage;
  }

  get totalPages() {
    return this._totalPages;
  }

  get isInteractable() {
    return this.state === CLOSED || this.state === OPEN;
  }

  get isOpen() {
    return this.state === OPEN;
  }

  get isClosed() {
    return this.state === CLOSED;
  }

  dispose() {
    for (const page of this._pages) {
      page.mesh.geometry.dispose();
      page.material.dispose();
    }
    if (this._coverFront) {
      this._coverFront.geometry.dispose();
      this._coverFront.material.dispose();
    }
    if (this._coverBack) {
      this._coverBack.geometry.dispose();
      this._coverBack.material.dispose();
    }
    if (this._spine) {
      this._spine.geometry.dispose();
      this._spine.material.dispose();
    }
  }
}
