import * as THREE from 'three';
import { patchMaterial } from '../fx/PSXEffect.js';

export default class PageMaterial {
  static create(frontTexture, backTexture) {
    // NearestFilter for PS1 pixelated textures
    if (frontTexture) {
      frontTexture.minFilter = THREE.NearestFilter;
      frontTexture.magFilter = THREE.NearestFilter;
      frontTexture.generateMipmaps = false;
    }
    if (backTexture) {
      backTexture.minFilter = THREE.NearestFilter;
      backTexture.magFilter = THREE.NearestFilter;
      backTexture.generateMipmaps = false;
    }

    const material = new THREE.MeshLambertMaterial({
      side: THREE.DoubleSide,
      map: frontTexture,
    });

    material.userData.frontTexture = frontTexture;
    material.userData.backTexture = backTexture;

    material.onBeforeCompile = (shader) => {
      shader.uniforms.frontTexture = { value: frontTexture };
      shader.uniforms.backTexture = { value: backTexture };

      shader.fragmentShader = shader.fragmentShader.replace(
        '#include <common>',
        `
        #include <common>
        uniform sampler2D frontTexture;
        uniform sampler2D backTexture;
        `
      );

      shader.fragmentShader = shader.fragmentShader.replace(
        '#include <map_fragment>',
        `
        #ifdef USE_MAP
          vec2 pageUv = vMapUv / vPsxW;
          vec4 sampledDiffuseColor;
          if (gl_FrontFacing) {
            sampledDiffuseColor = texture2D(frontTexture, pageUv);
          } else {
            vec2 backUv = vec2(1.0 - pageUv.x, pageUv.y);
            sampledDiffuseColor = texture2D(backTexture, backUv);
          }
          diffuseColor *= sampledDiffuseColor;
        #endif
        `
      );
    };

    patchMaterial(material);
    return material;
  }
}
