import * as THREE from 'three';
import { FBXLoader } from 'three/addons/loaders/FBXLoader.js';

const cache = new Map();
const fbxLoader = new FBXLoader();

/**
 * Loads an FBX model. Normalizes unit scale if needed.
 * Caches by URL — returns a clone each time.
 */
export async function loadFBX(url) {
  if (cache.has(url)) {
    return cache.get(url).clone();
  }

  const group = await fbxLoader.loadAsync(url);

  // FBX files from Blender are often in centimeters — auto-detect and fix
  const box = new THREE.Box3().setFromObject(group);
  const height = box.getSize(new THREE.Vector3()).y;
  if (height > 50) {
    group.scale.multiplyScalar(0.01);
  }

  // Log mesh info for debugging
  group.traverse((child) => {
    if (child.isMesh) {
      console.log('[ModelLoader] Mesh:', child.name,
        'type:', child.type,
        'material:', child.material?.type,
        'hasMap:', !!child.material?.map,
        'hasSkeleton:', !!child.skeleton);

      // Apply NearestFilter to any textures for PSX look
      const mat = child.material;
      if (mat.map) {
        mat.map.minFilter = THREE.NearestFilter;
        mat.map.magFilter = THREE.NearestFilter;
        mat.map.generateMipmaps = false;
      }

      child.castShadow = false;
      child.receiveShadow = true;
    }
  });

  cache.set(url, group);
  return group.clone();
}

export function disposeCache() {
  for (const group of cache.values()) {
    group.traverse((child) => {
      if (child.isMesh) {
        child.geometry.dispose();
        if (child.material) {
          if (child.material.map) child.material.map.dispose();
          child.material.dispose();
        }
      }
    });
  }
  cache.clear();
}
