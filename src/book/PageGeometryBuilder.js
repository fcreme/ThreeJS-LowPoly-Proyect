import * as THREE from 'three';

/**
 * Creates a SkinnedMesh page with a 6-bone chain along the X-axis for curl deformation.
 */
export default class PageGeometryBuilder {
  constructor() {
    this.boneCount = 6;
    this.pageWidth = 0.20;
    this.pageHeight = 0.28;
    this.segmentsX = 20;
    this.segmentsY = 10;
  }

  /**
   * @returns {{ mesh: THREE.SkinnedMesh, bones: THREE.Bone[], skeleton: THREE.Skeleton }}
   */
  build(material) {
    const geometry = new THREE.PlaneGeometry(
      this.pageWidth, this.pageHeight,
      this.segmentsX, this.segmentsY
    );

    // Create bone chain along X-axis
    const bones = [];
    const boneSpacing = this.pageWidth / (this.boneCount - 1);

    for (let i = 0; i < this.boneCount; i++) {
      const bone = new THREE.Bone();
      bone.name = `page_bone_${i}`;

      if (i === 0) {
        bone.position.set(-this.pageWidth / 2, 0, 0);
      } else {
        bone.position.set(boneSpacing, 0, 0);
      }

      if (i > 0) {
        bones[i - 1].add(bone);
      }
      bones.push(bone);
    }

    // Compute skin weights
    const position = geometry.attributes.position;
    const skinIndices = new Uint16Array(position.count * 4);
    const skinWeights = new Float32Array(position.count * 4);

    for (let i = 0; i < position.count; i++) {
      const x = position.getX(i);
      // Normalized position along page width (0 = left edge, 1 = right edge)
      const normalizedX = (x + this.pageWidth / 2) / this.pageWidth;

      // Find the two closest bones
      const bonePos = normalizedX * (this.boneCount - 1);
      const boneIdx0 = Math.floor(bonePos);
      const boneIdx1 = Math.min(boneIdx0 + 1, this.boneCount - 1);
      const weight1 = bonePos - boneIdx0;
      const weight0 = 1 - weight1;

      skinIndices[i * 4] = boneIdx0;
      skinIndices[i * 4 + 1] = boneIdx1;
      skinIndices[i * 4 + 2] = 0;
      skinIndices[i * 4 + 3] = 0;

      skinWeights[i * 4] = weight0;
      skinWeights[i * 4 + 1] = weight1;
      skinWeights[i * 4 + 2] = 0;
      skinWeights[i * 4 + 3] = 0;
    }

    geometry.setAttribute('skinIndex', new THREE.Uint16BufferAttribute(skinIndices, 4));
    geometry.setAttribute('skinWeight', new THREE.BufferAttribute(skinWeights, 4));

    // Create skeleton
    const skeleton = new THREE.Skeleton(bones);

    // Create skinned mesh
    const mesh = new THREE.SkinnedMesh(geometry, material);
    mesh.add(bones[0]);
    mesh.bind(skeleton);
    mesh.castShadow = false;
    mesh.receiveShadow = false;
    mesh.frustumCulled = false;

    // Disable SkinnedMesh raycast — it applies bone transforms to every vertex
    // of every triangle, which is extremely expensive. The book uses a simple
    // hitbox for interaction raycasting instead.
    mesh.raycast = function () {};

    // Pre-compute bounding sphere from geometry to prevent the renderer from
    // calling SkinnedMesh.computeBoundingSphere() via the sortObjects path,
    // which requires bone world matrices that aren't ready until the first scene traversal.
    mesh.geometry.computeBoundingSphere();
    mesh.boundingSphere = mesh.geometry.boundingSphere.clone();

    return { mesh, bones, skeleton };
  }
}
