import { gaussian, easeInOutCubic } from '../utils/math.js';

/**
 * Drives bone rotations for a page turn animation.
 * Single parameter t (0→1) controls the full turn.
 */
export default class PageCurlAnimator {
  constructor(bones) {
    this.bones = bones;
    this.boneCount = bones.length;
    this._totalRotation = Math.PI; // 180 degrees for full turn
  }

  /**
   * Update bone rotations for a given progress value.
   * @param {number} t - Progress from 0 (flat on right) to 1 (flat on left)
   * @param {number} elapsed - Total elapsed time for micro-animations
   */
  update(t, elapsed = 0) {
    const easedT = easeInOutCubic(t);

    for (let i = 1; i < this.boneCount; i++) {
      const bone = this.bones[i];
      const boneNormalized = i / (this.boneCount - 1);

      // Wave position — how much this bone should have rotated
      const waveCenter = easedT;
      const distFromWave = boneNormalized - waveCenter;

      let rotation = 0;

      if (distFromWave < -0.15) {
        // Behind the wave — fully turned
        rotation = this._totalRotation / (this.boneCount - 1);
      } else if (distFromWave > 0.3) {
        // Ahead of the wave — not yet reached
        rotation = 0;
      } else {
        // At the wave — partial rotation with Gaussian curl
        const curlAmount = gaussian(distFromWave, 0, 0.18);
        rotation = curlAmount * this._totalRotation / (this.boneCount - 1);

        // Also accumulate rotation for bones that should be partially turned
        if (distFromWave < 0) {
          const partialTurn = 1 - (distFromWave + 0.15) / 0.15;
          rotation += partialTurn * this._totalRotation / (this.boneCount - 1);
        }
      }

      // Gravity droop — slight Y sag when page is near vertical (t≈0.5)
      const verticalAmount = Math.sin(easedT * Math.PI) * 0.03;
      const sagY = verticalAmount * gaussian(boneNormalized, easedT, 0.3);

      // Air wobble — micro oscillation during turn
      const wobble = t > 0.05 && t < 0.95
        ? Math.sin(elapsed * 15 + boneNormalized * 5) * 0.01 * Math.sin(t * Math.PI)
        : 0;

      bone.rotation.z = -(rotation + wobble);
      bone.rotation.x = sagY;
    }
  }

  /**
   * Reset all bones to flat position.
   */
  reset() {
    for (let i = 1; i < this.boneCount; i++) {
      this.bones[i].rotation.set(0, 0, 0);
    }
  }

  /**
   * Set bones to fully turned position.
   */
  setFullyTurned() {
    const rotPerBone = this._totalRotation / (this.boneCount - 1);
    for (let i = 1; i < this.boneCount; i++) {
      this.bones[i].rotation.z = -rotPerBone;
    }
  }
}
