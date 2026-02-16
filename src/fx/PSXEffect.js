import * as THREE from 'three';

// --- PSX Constants ---
export const PSX_RENDER_HEIGHT = 480;
export const PSX_FOG_DENSITY = 0.018;
export const PSX_CAMERA_FAR = 120;
export const PSX_EXPOSURE = 1.4;
export const PSX_GRID_RESOLUTION = 320.0;
export const PSX_COLOR_DEPTH = 64.0;
export const PSX_DITHER_STRENGTH = 0.012;
export const PSX_SCANLINE_INTENSITY = 0.04;

/**
 * Patches any Three.js material (Lambert, Phong, Standard) with PS1 vertex jitter.
 * Chains onto existing onBeforeCompile safely.
 */
export function patchMaterial(material) {
  const existingCallback = material.onBeforeCompile;

  material.onBeforeCompile = (shader, renderer) => {
    if (existingCallback) {
      existingCallback(shader, renderer);
    }

    shader.vertexShader = shader.vertexShader.replace(
      '#include <common>',
      `#include <common>
       varying float vPsxW;`
    );

    shader.vertexShader = shader.vertexShader.replace(
      '#include <project_vertex>',
      `#include <project_vertex>
       float psxGrid = ${PSX_GRID_RESOLUTION.toFixed(1)};
       gl_Position.xy = floor(gl_Position.xy * psxGrid / gl_Position.w + 0.5) * gl_Position.w / psxGrid;
       vPsxW = gl_Position.w;`
    );

    shader.fragmentShader = shader.fragmentShader.replace(
      '#include <common>',
      `#include <common>
       varying float vPsxW;`
    );
  };

  material.needsUpdate = true;
  return material;
}

/**
 * Forces NearestFilter on a texture for PS1 pixelated look.
 */
export function psxTexture(texture) {
  if (!texture) return texture;
  texture.minFilter = THREE.NearestFilter;
  texture.magFilter = THREE.NearestFilter;
  texture.generateMipmaps = false;
  texture.needsUpdate = true;
  return texture;
}

// --- PSX Color Shader (posterization + ordered dithering) ---
export const PSXColorShader = {
  uniforms: {
    tDiffuse: { value: null },
    colorDepth: { value: PSX_COLOR_DEPTH },
    ditherStrength: { value: PSX_DITHER_STRENGTH },
    resolution: { value: new THREE.Vector2(320, 240) },
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
    uniform float colorDepth;
    uniform float ditherStrength;
    uniform vec2 resolution;
    varying vec2 vUv;

    float bayer4x4(vec2 pos) {
      ivec2 p = ivec2(mod(pos, 4.0));
      int idx = p.x + p.y * 4;
      if (idx == 0)  return  0.0 / 16.0;
      if (idx == 1)  return  8.0 / 16.0;
      if (idx == 2)  return  2.0 / 16.0;
      if (idx == 3)  return 10.0 / 16.0;
      if (idx == 4)  return 12.0 / 16.0;
      if (idx == 5)  return  4.0 / 16.0;
      if (idx == 6)  return 14.0 / 16.0;
      if (idx == 7)  return  6.0 / 16.0;
      if (idx == 8)  return  3.0 / 16.0;
      if (idx == 9)  return 11.0 / 16.0;
      if (idx == 10) return  1.0 / 16.0;
      if (idx == 11) return  9.0 / 16.0;
      if (idx == 12) return 15.0 / 16.0;
      if (idx == 13) return  7.0 / 16.0;
      if (idx == 14) return 13.0 / 16.0;
      return 5.0 / 16.0;
    }

    void main() {
      vec4 color = texture2D(tDiffuse, vUv);
      vec2 pixelPos = vUv * resolution;
      float dither = (bayer4x4(pixelPos) - 0.5) * ditherStrength;
      color.rgb += dither;
      color.rgb = floor(color.rgb * colorDepth + 0.5) / colorDepth;
      gl_FragColor = color;
    }
  `,
};

// --- Scanline Shader ---
export const ScanlineShader = {
  uniforms: {
    tDiffuse: { value: null },
    scanlineIntensity: { value: PSX_SCANLINE_INTENSITY },
    scanlineCount: { value: 480.0 },
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
    uniform float scanlineIntensity;
    uniform float scanlineCount;
    varying vec2 vUv;
    void main() {
      vec4 color = texture2D(tDiffuse, vUv);
      float scanline = sin(vUv.y * scanlineCount * 3.14159) * 0.5 + 0.5;
      color.rgb *= 1.0 - scanlineIntensity * (1.0 - scanline);
      gl_FragColor = color;
    }
  `,
};

// --- Desaturation Shader (PS1 muted palette) ---
export const DesaturationShader = {
  uniforms: {
    tDiffuse: { value: null },
    amount: { value: 0.35 },
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
    uniform float amount;
    varying vec2 vUv;
    void main() {
      vec4 color = texture2D(tDiffuse, vUv);
      float lum = dot(color.rgb, vec3(0.299, 0.587, 0.114));
      color.rgb = mix(color.rgb, vec3(lum), amount);
      gl_FragColor = color;
    }
  `,
};
