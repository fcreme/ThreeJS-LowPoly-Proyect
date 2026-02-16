import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { ShaderPass } from 'three/addons/postprocessing/ShaderPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { PSXColorShader, ScanlineShader, DesaturationShader } from './PSXEffect.js';

const VignetteShader = {
  uniforms: {
    tDiffuse: { value: null },
    offset: { value: 1.2 },
    darkness: { value: 1.5 },
  },
  vertexShader: /* glsl */ `
    varying vec2 vUv;
    void main() {
      vUv = uv;
      gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
    }
  `,
  fragmentShader: /* glsl */ `
    uniform sampler2D tDiffuse;
    uniform float offset;
    uniform float darkness;
    varying vec2 vUv;
    void main() {
      vec4 texel = texture2D(tDiffuse, vUv);
      vec2 uv = (vUv - vec2(0.5)) * vec2(offset);
      texel.rgb *= clamp(1.0 - dot(uv, uv), 0.0, 1.0) * darkness + (1.0 - darkness);
      gl_FragColor = texel;
    }
  `,
};

const FilmShader = {
  uniforms: {
    tDiffuse: { value: null },
    time: { value: 0.0 },
    intensity: { value: 0.12 },
    grayscale: { value: false },
  },
  vertexShader: /* glsl */ `
    varying vec2 vUv;
    void main() {
      vUv = uv;
      gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
    }
  `,
  fragmentShader: /* glsl */ `
    uniform sampler2D tDiffuse;
    uniform float time;
    uniform float intensity;
    uniform bool grayscale;
    varying vec2 vUv;
    float rand(vec2 co) {
      return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
    }
    void main() {
      vec4 texel = texture2D(tDiffuse, vUv);
      float noise = rand(vUv + vec2(time)) * intensity;
      texel.rgb += vec3(noise) - vec3(intensity * 0.5);
      gl_FragColor = texel;
    }
  `,
};

export default class PostProcessing {
  constructor(renderer, scene, camera) {
    this._renderer = renderer;
    const size = renderer.getSize(new THREE.Vector2());

    this._composer = new EffectComposer(renderer);

    // 1. Render
    this._composer.addPass(new RenderPass(scene, camera));

    // 2. PSX Color (posterize + dither)
    this._psxColor = new ShaderPass(PSXColorShader);
    this._psxColor.uniforms.resolution.value.set(size.x, size.y);
    this._composer.addPass(this._psxColor);

    // 3. Bloom — warm light glow
    this._bloom = new UnrealBloomPass(
      new THREE.Vector2(size.x, size.y),
      0.45,  // strength
      0.9,   // radius
      0.5    // threshold
    );
    this._composer.addPass(this._bloom);

    // 4. Desaturation — slight PS1 muted palette
    this._desat = new ShaderPass(DesaturationShader);
    this._desat.uniforms.amount.value = 0.2;
    this._composer.addPass(this._desat);

    // 5. Scanlines
    this._scanlines = new ShaderPass(ScanlineShader);
    this._scanlines.uniforms.scanlineCount.value = size.y;
    this._composer.addPass(this._scanlines);

    // 6. Vignette — subtle edge darkening only
    this._vignette = new ShaderPass(VignetteShader);
    this._vignette.uniforms.offset.value = 1.0;
    this._vignette.uniforms.darkness.value = 0.6;
    this._composer.addPass(this._vignette);

    // 7. Film grain
    this._film = new ShaderPass(FilmShader);
    this._film.uniforms.intensity.value = 0.02;
    this._composer.addPass(this._film);

    // 8. Output
    this._composer.addPass(new OutputPass());
  }

  update(elapsed) {
    this._film.uniforms.time.value = elapsed;
  }

  render() {
    this._composer.render();
  }

  resize(w, h) {
    this._composer.setSize(w, h);
    this._psxColor.uniforms.resolution.value.set(w, h);
    this._scanlines.uniforms.scanlineCount.value = h;
  }

  dispose() {
    this._composer.passes.forEach(pass => {
      if (pass.dispose) pass.dispose();
    });
  }
}
