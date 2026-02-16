import * as THREE from 'three';

class AssetPipeline {
  constructor() {
    this._textureLoader = new THREE.TextureLoader();
    this._cache = new Map();
  }

  loadTexture(url, options = {}) {
    if (this._cache.has(url)) {
      return Promise.resolve(this._cache.get(url));
    }
    return new Promise((resolve, reject) => {
      this._textureLoader.load(
        url,
        (tex) => {
          if (options.srgb !== false) {
            tex.colorSpace = THREE.SRGBColorSpace;
          }
          if (options.wrapS) tex.wrapS = options.wrapS;
          if (options.wrapT) tex.wrapT = options.wrapT;
          if (options.repeat) tex.repeat.set(options.repeat[0], options.repeat[1]);

          // PS1: NearestFilter on all textures
          tex.minFilter = THREE.NearestFilter;
          tex.magFilter = THREE.NearestFilter;
          tex.generateMipmaps = false;

          this._cache.set(url, tex);
          resolve(tex);
        },
        undefined,
        reject
      );
    });
  }

  get(url) {
    return this._cache.get(url);
  }

  dispose() {
    for (const tex of this._cache.values()) {
      tex.dispose();
    }
    this._cache.clear();
  }
}

export default new AssetPipeline();
