export function lerp(a, b, t) {
  return a + (b - a) * t;
}

export function easeInOutCubic(t) {
  return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
}

export function randomRange(min, max) {
  return min + Math.random() * (max - min);
}

export function gaussian(x, mean, sigma) {
  return Math.exp(-0.5 * Math.pow((x - mean) / sigma, 2));
}
