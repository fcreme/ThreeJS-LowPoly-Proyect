import * as THREE from 'three';
import Book from './Book.js';
import PageTextureCompositor from './PageTextureCompositor.js';
import AssetPipeline from '../core/AssetPipeline.js';

export default class BookFactory {
  constructor() {
    this._compositor = new PageTextureCompositor();
    this._textureLoader = new THREE.TextureLoader();
  }

  /**
   * Create a book from an artist config.
   * @param {Object} config - { slug, title, pages: [{image, title, description}], pathT, side }
   * @returns {Promise<Book>}
   */
  async create(config) {
    // Load cover texture
    let coverTexture = null;
    if (config.cover) {
      try {
        coverTexture = await this._loadImage(config.cover);
      } catch (e) {
        console.warn(`Could not load cover for ${config.slug}:`, e);
      }
    }

    // Create cover texture with title (smaller — text only)
    const coverCanvasTex = this._compositor.composeText(
      config.description || '',
      config.title || 'Untitled',
      512, 512
    );

    // Create page textures
    const pageTextures = [];
    const textSize = 512; // Text-only pages don't need 1024

    if (config.pages && config.pages.length > 0) {
      for (const page of config.pages) {
        // Front: artwork or text
        if (page.image) {
          try {
            const img = await this._loadImageElement(page.image);
            const frontTex = this._compositor.compose(img, 1024, 1024);
            pageTextures.push(frontTex);
          } catch (e) {
            console.warn(`Could not load page image ${page.image}:`, e);
            const frontTex = this._compositor.composeText(page.description || '', page.title || '', textSize, textSize);
            pageTextures.push(frontTex);
          }
        } else {
          const frontTex = this._compositor.composeText(page.description || '', page.title || '', textSize, textSize);
          pageTextures.push(frontTex);
        }

        // Back: text info or blank
        const backTex = this._compositor.composeText(
          page.description || '',
          page.title || '',
          textSize, textSize
        );
        pageTextures.push(backTex);
      }
    } else {
      // Default pages if none configured
      const defaultPages = [
        { title: config.title || 'Artist', description: 'Portfolio' },
        { title: 'About', description: config.description || 'A collection of works.' },
      ];
      for (const p of defaultPages) {
        pageTextures.push(this._compositor.composeText(p.description, p.title, textSize, textSize));
        pageTextures.push(this._compositor.composeText('', '', textSize, textSize));
      }
    }

    const book = new Book(config, pageTextures, coverCanvasTex);
    return book;
  }

  _loadImage(url) {
    return AssetPipeline.loadTexture(url);
  }

  _loadImageElement(url) {
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.crossOrigin = 'anonymous';
      img.onload = () => resolve(img);
      img.onerror = reject;
      img.src = url;
    });
  }
}
