import * as THREE from 'three';

export default class PageTextureCompositor {
  constructor() {
    this._canvas = document.createElement('canvas');
    this._ctx = this._canvas.getContext('2d');
  }

  /**
   * Composites artwork onto a paper texture.
   * @param {HTMLImageElement|null} artworkImage - The artwork image (null for blank page)
   * @param {number} width - Canvas width (default 1024)
   * @param {number} height - Canvas height (default 1024)
   * @returns {THREE.CanvasTexture}
   */
  compose(artworkImage, width = 1024, height = 1024) {
    this._canvas.width = width;
    this._canvas.height = height;
    const ctx = this._ctx;

    // 1. Paper base (parchment color)
    ctx.fillStyle = '#d4c5a9';
    ctx.fillRect(0, 0, width, height);

    // Paper grain noise
    this._drawPaperGrain(ctx, width, height);

    // 2. Draw artwork with margins
    if (artworkImage) {
      const margin = Math.floor(width * 0.12);
      const artW = width - margin * 2;
      const artH = height - margin * 2;

      ctx.save();
      ctx.globalAlpha = 0.92;
      ctx.drawImage(artworkImage, margin, margin, artW, artH);
      ctx.restore();

      // 3. Multiply-blend paper grain over artwork
      ctx.save();
      ctx.globalAlpha = 0.15;
      ctx.globalCompositeOperation = 'multiply';
      this._drawPaperGrain(ctx, width, height);
      ctx.restore();
    }

    // 4. Edge vignette
    this._drawVignette(ctx, width, height);

    const texture = new THREE.CanvasTexture(this._canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    // PS1: NearestFilter on all textures
    texture.minFilter = THREE.NearestFilter;
    texture.magFilter = THREE.NearestFilter;
    texture.generateMipmaps = false;

    // Create a new canvas for next use
    this._canvas = document.createElement('canvas');
    this._ctx = this._canvas.getContext('2d');

    return texture;
  }

  composeText(text, title = '', width = 1024, height = 1024) {
    this._canvas.width = width;
    this._canvas.height = height;
    const ctx = this._ctx;

    // Paper base
    ctx.fillStyle = '#d4c5a9';
    ctx.fillRect(0, 0, width, height);
    this._drawPaperGrain(ctx, width, height);

    // Title
    const margin = Math.floor(width * 0.12);
    if (title) {
      ctx.fillStyle = '#2a1a0a';
      ctx.font = `bold ${Math.floor(width * 0.045)}px Georgia, serif`;
      ctx.textAlign = 'center';
      ctx.fillText(title, width / 2, margin + 40);
    }

    // Body text
    if (text) {
      ctx.fillStyle = '#3a2a1a';
      ctx.font = `${Math.floor(width * 0.028)}px Georgia, serif`;
      ctx.textAlign = 'left';

      const lineHeight = Math.floor(width * 0.04);
      const maxWidth = width - margin * 2;
      const words = text.split(' ');
      let line = '';
      let y = margin + (title ? 80 : 40);

      for (const word of words) {
        const testLine = line + word + ' ';
        if (ctx.measureText(testLine).width > maxWidth) {
          ctx.fillText(line.trim(), margin, y);
          line = word + ' ';
          y += lineHeight;
        } else {
          line = testLine;
        }
      }
      ctx.fillText(line.trim(), margin, y);
    }

    this._drawVignette(ctx, width, height);

    const texture = new THREE.CanvasTexture(this._canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    // PS1: NearestFilter on all textures
    texture.minFilter = THREE.NearestFilter;
    texture.magFilter = THREE.NearestFilter;
    texture.generateMipmaps = false;

    this._canvas = document.createElement('canvas');
    this._ctx = this._canvas.getContext('2d');

    return texture;
  }

  _drawPaperGrain(ctx, w, h) {
    // Use scattered semi-transparent rects instead of per-pixel noise — orders of magnitude faster
    ctx.save();
    const count = Math.floor(w * h * 0.003); // ~3000 specks for 1024x1024
    for (let i = 0; i < count; i++) {
      const x = Math.random() * w;
      const y = Math.random() * h;
      const size = 1 + Math.random() * 2;
      const bright = Math.random() > 0.5;
      ctx.fillStyle = bright
        ? `rgba(255,255,255,${0.03 + Math.random() * 0.04})`
        : `rgba(0,0,0,${0.03 + Math.random() * 0.04})`;
      ctx.fillRect(x, y, size, size);
    }
    ctx.restore();
  }

  _drawVignette(ctx, w, h) {
    const gradient = ctx.createRadialGradient(
      w / 2, h / 2, w * 0.3,
      w / 2, h / 2, w * 0.7
    );
    gradient.addColorStop(0, 'rgba(0,0,0,0)');
    gradient.addColorStop(1, 'rgba(0,0,0,0.2)');

    ctx.save();
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, w, h);
    ctx.restore();
  }
}
