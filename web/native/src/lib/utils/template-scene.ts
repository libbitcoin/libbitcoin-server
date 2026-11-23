import {
  chunkRate,
  formatTemplateRate,
  ratePosition,
  rateBlend,
  templateName,
  templateRates,
  templateTiles,
  transactionRate,
  type PoolTemplates,
  type RateBlend,
} from './pool-template';
import { formatInteger } from './format';

// Templates drawn in 3D on a 2D canvas: each template is a platform, each
// transaction a box on it. The footprint is the 2D treemap tile, so area is
// virtual size; height follows the selected mode.

export type HeightMode = 'chunk' | 'own' | 'fee';

export type Rgb = readonly [number, number, number];

export interface ScenePalette {
  surface: Rgb;
  accent: Rgb;
  text: Rgb;
  muted: Rgb;
  font: string;
}

export function parseHexColor(value: string, fallback: Rgb): Rgb {
  const hex = /^#([\da-f]{3}|[\da-f]{6})$/i.exec(value.trim())?.[1];
  if (!hex) return fallback;
  const full = hex.length === 3 ? [...hex].map((digit) => digit + digit).join('') : hex;
  return [0, 2, 4].map((offset) => parseInt(full.slice(offset, offset + 2), 16)) as unknown as Rgb;
}

// The canvas counterpart of color-mix: share percent of the tint over the base.
export function mixColor(tint: Rgb, base: Rgb, share: number, shade = 1): string {
  const part = (index: number) =>
    String(Math.round(((tint[index] * share) / 100 + (base[index] * (100 - share)) / 100) * shade));
  return `rgb(${part(0)},${part(1)},${part(2)})`;
}

const rgba = (color: Rgb, alpha: number) =>
  `rgba(${String(color[0])},${String(color[1])},${String(color[2])},${String(alpha)})`;

const WIDTH = 16;
const DEPTH = 10;
const GAP = 4;
const TALLEST = 4.2;
const LOWEST = 0.12;
const PLINTH = 0.35;

interface Box {
  transaction: number;
  x0: number;
  x1: number;
  z0: number;
  z1: number;
  heights: Record<HeightMode, number>;
  blend: RateBlend;
  top: string;
  sideX: string;
  sideZ: string;
}

export interface Camera {
  azimuth: number;
  elevation: number;
  distance: number;
  x: number;
  y: number;
  z: number;
}

interface Drag {
  x: number;
  y: number;
  startX: number;
  startY: number;
  pan: boolean;
  moved: boolean;
}

type Face = number[];

function templateCenter(template: number): number {
  return template * (WIDTH + GAP) + WIDTH / 2;
}

function homeCamera(template: number): Camera {
  return { azimuth: -0.62, elevation: 0.6, distance: 22, x: templateCenter(template), y: 0.6, z: DEPTH / 2 };
}

function inside(x: number, y: number, face: Face): boolean {
  let crossing = false;
  for (let i = 0, j = face.length - 2; i < face.length; j = i, i += 2) {
    const [xi, yi, xj, yj] = [face[i], face[i + 1], face[j], face[j + 1]];
    if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) crossing = !crossing;
  }
  return crossing;
}

export class TemplateScene {
  private readonly context: CanvasRenderingContext2D;
  private readonly observer: ResizeObserver;
  private pool: PoolTemplates | null = null;
  private boxes: Box[] = [];
  private heights = new Float32Array(0);
  private order = new Uint32Array(0);
  private depths = new Float32Array(0);
  private hits: Array<{ box: number; faces: Face[] }> = [];
  private labels: Array<{ title: string; detail: string }> = [];
  private camera = homeCamera(0);
  private goal: { x: number; z: number } | null = null;
  private mode: HeightMode = 'chunk';
  private template = 0;
  private selected = -1;
  private hover = -1;
  private pointer: [number, number] | null = null;
  private drag: Drag | null = null;
  private frame = 0;
  private ratio = 1;
  private palette: ScenePalette = {
    surface: [31, 33, 28],
    accent: [255, 160, 34],
    text: [237, 237, 237],
    muted: [207, 207, 207],
    font: 'sans-serif',
  };

  constructor(
    private readonly canvas: HTMLCanvasElement,
    private readonly onSelect: (transaction: number) => void,
    private readonly animate: boolean
  ) {
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Canvas 2D context unavailable.');
    this.context = context;
    canvas.addEventListener('pointerdown', this.pointerDown);
    canvas.addEventListener('pointermove', this.pointerMove);
    canvas.addEventListener('pointerup', this.pointerUp);
    canvas.addEventListener('pointerleave', this.pointerLeave);
    canvas.addEventListener('contextmenu', this.contextMenu);
    canvas.addEventListener('wheel', this.wheel, { passive: false });
    canvas.addEventListener('keydown', this.keyDown);
    this.observer = new ResizeObserver(this.resize);
    this.observer.observe(canvas);
    this.resize();
  }

  destroy(): void {
    cancelAnimationFrame(this.frame);
    this.observer.disconnect();
    this.canvas.removeEventListener('pointerdown', this.pointerDown);
    this.canvas.removeEventListener('pointermove', this.pointerMove);
    this.canvas.removeEventListener('pointerup', this.pointerUp);
    this.canvas.removeEventListener('pointerleave', this.pointerLeave);
    this.canvas.removeEventListener('contextmenu', this.contextMenu);
    this.canvas.removeEventListener('wheel', this.wheel);
    this.canvas.removeEventListener('keydown', this.keyDown);
  }

  setPool(pool: PoolTemplates): void {
    this.pool = pool;
    const largest = Math.max(1, ...pool.transactions.map((transaction) => transaction.fee));
    const boxes: Box[] = [];
    pool.templates.forEach((_, template) => {
      const offset = template * (WIDTH + GAP);
      for (const { transaction, rect } of templateTiles(pool, template, WIDTH, DEPTH)) {
        const entry = pool.transactions[transaction];
        // Keep a seam between boxes without swallowing the smallest ones.
        const seamX = Math.min(0.03, rect.width * 0.15);
        const seamZ = Math.min(0.03, rect.height * 0.15);
        const scale = (position: number) => LOWEST + TALLEST * position;
        boxes.push({
          transaction,
          x0: offset + rect.x + seamX,
          x1: offset + rect.x + rect.width - seamX,
          z0: rect.y + seamZ,
          z1: rect.y + rect.height - seamZ,
          heights: {
            chunk: scale(ratePosition(chunkRate(pool.chunks[entry.chunk]))),
            own: scale(ratePosition(transactionRate(entry))),
            fee: scale(Math.max(0, Math.log(entry.fee / 100) / Math.log(largest / 100))),
          },
          blend: rateBlend(transactionRate(entry)),
          top: '',
          sideX: '',
          sideZ: '',
        });
      }
    });

    this.boxes = boxes;
    this.heights = Float32Array.from(boxes, (box) => box.heights[this.mode]);
    this.order = Uint32Array.from(boxes, (_, index) => index);
    this.depths = new Float32Array(boxes.length);
    this.labels = pool.templates.map((template, index) => ({
      title: templateName(pool, index),
      detail: `~${formatTemplateRate(templateRates(pool, template).median)} sat/vB · ${formatInteger(template.count)} txs`,
    }));
    this.hover = -1;
    this.paint();
    this.request();
  }

  setPalette(palette: ScenePalette): void {
    this.palette = palette;
    this.paint();
    this.request();
  }

  setMode(mode: HeightMode): void {
    this.mode = mode;
    if (!this.animate) this.heights = Float32Array.from(this.boxes, (box) => box.heights[mode]);
    this.request();
  }

  setSelection(template: number, transaction: number | null): void {
    if (template !== this.template) {
      this.template = template;
      this.glide(template);
    }
    this.selected = transaction ?? -1;
    this.request();
  }

  getView(): Camera {
    return { ...this.camera, ...this.goal };
  }

  setView(camera: Camera): void {
    this.camera = { ...camera };
    this.goal = null;
    this.request();
  }

  resetView(): void {
    this.camera = homeCamera(this.template);
    this.goal = null;
    this.request();
  }

  private paint(): void {
    for (const box of this.boxes) {
      const [tint, base, share] = [this.palette[box.blend.tint], this.palette[box.blend.base], box.blend.share];
      box.top = mixColor(tint, base, share);
      box.sideX = mixColor(tint, base, share, 0.62);
      box.sideZ = mixColor(tint, base, share, 0.8);
    }
  }

  private glide(template: number): void {
    const goal = { x: templateCenter(template), z: DEPTH / 2 };
    if (this.animate) this.goal = goal;
    else Object.assign(this.camera, goal);
  }

  private request(): void {
    if (this.frame) return;
    this.frame = requestAnimationFrame(() => {
      this.frame = 0;
      if (this.step()) this.request();
    });
  }

  // Advance animations and draw; true while anything is still moving.
  private step(): boolean {
    let moving = false;
    const camera = this.camera;
    if (this.goal) {
      camera.x += (this.goal.x - camera.x) * 0.14;
      camera.z += (this.goal.z - camera.z) * 0.14;
      if (Math.abs(this.goal.x - camera.x) + Math.abs(this.goal.z - camera.z) < 0.01) {
        Object.assign(camera, this.goal);
        this.goal = null;
      } else moving = true;
    }

    for (let index = 0; index < this.boxes.length; index += 1) {
      const target = this.boxes[index].heights[this.mode];
      const delta = target - this.heights[index];
      if (Math.abs(delta) > 0.002) {
        this.heights[index] += delta * 0.16;
        moving = true;
      } else this.heights[index] = target;
    }

    this.draw();
    return moving;
  }

  private readonly resize = (): void => {
    this.ratio = Math.min(2, globalThis.devicePixelRatio || 1);
    this.canvas.width = Math.max(1, Math.round(this.canvas.clientWidth * this.ratio));
    this.canvas.height = Math.max(1, Math.round(this.canvas.clientHeight * this.ratio));
    this.request();
  };

  private draw(): void {
    const { context, camera, palette, boxes, heights } = this;
    const width = this.canvas.width / this.ratio;
    const height = this.canvas.height / this.ratio;
    context.setTransform(this.ratio, 0, 0, this.ratio, 0, 0);
    context.fillStyle = rgba(palette.surface, 1);
    context.fillRect(0, 0, width, height);
    if (!this.pool) return;

    const level = Math.cos(camera.elevation);
    const ex = camera.x + camera.distance * level * Math.sin(camera.azimuth);
    const ey = camera.y + camera.distance * Math.sin(camera.elevation);
    const ez = camera.z + camera.distance * level * Math.cos(camera.azimuth);
    let [fx, fy, fz] = [camera.x - ex, camera.y - ey, camera.z - ez];
    const length = Math.hypot(fx, fy, fz);
    [fx, fy, fz] = [fx / length, fy / length, fz / length];
    const across = Math.hypot(fz, fx);
    const [rx, rz] = [-fz / across, fx / across];
    const [ux, uy, uz] = [-rz * fy, rz * fx - rx * fz, rx * fy];
    const focal = height * 1.15;
    const [cx, cy] = [width / 2, height / 2];

    const project = (x: number, y: number, z: number, out: number[], at: number): boolean => {
      const [vx, vy, vz] = [x - ex, y - ey, z - ez];
      const depth = vx * fx + vy * fy + vz * fz;
      if (depth < 0.2) return false;
      out[at] = cx + ((vx * rx + vz * rz) * focal) / depth;
      out[at + 1] = cy - ((vx * ux + vy * uy + vz * uz) * focal) / depth;
      return true;
    };
    const quad = (corners: number[]): Face | null => {
      const face = new Array<number>(8);
      for (let corner = 0; corner < 4; corner += 1)
        if (!project(corners[corner * 3], corners[corner * 3 + 1], corners[corner * 3 + 2], face, corner * 2))
          return null;
      return face;
    };
    const fill = (face: Face | null, color: string | null, stroke: string | null, line: number) => {
      if (!face) return;
      context.beginPath();
      context.moveTo(face[0], face[1]);
      for (let at = 2; at < face.length; at += 2) context.lineTo(face[at], face[at + 1]);
      context.closePath();
      if (color) {
        context.fillStyle = color;
        context.fill();
      }
      if (stroke) {
        context.strokeStyle = stroke;
        context.lineWidth = line;
        context.stroke();
      }
    };
    // Only the faces turned toward the camera are drawn.
    const prism = (
      [x0, x1, y0, y1, z0, z1]: number[],
      [top, sideX, sideZ]: string[],
      edge: string,
      faces?: Face[]
    ) => {
      const drawn: Array<[Face | null, string]> = [];
      if (ex > x1) drawn.push([quad([x1, y0, z0, x1, y1, z0, x1, y1, z1, x1, y0, z1]), sideX]);
      else if (ex < x0) drawn.push([quad([x0, y0, z0, x0, y1, z0, x0, y1, z1, x0, y0, z1]), sideX]);
      if (ez > z1) drawn.push([quad([x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1]), sideZ]);
      else if (ez < z0) drawn.push([quad([x0, y0, z0, x1, y0, z0, x1, y1, z0, x0, y1, z0]), sideZ]);
      if (ey > y1) drawn.push([quad([x0, y1, z0, x1, y1, z0, x1, y1, z1, x0, y1, z1]), top]);
      for (const [face, color] of drawn) {
        fill(face, color, edge, 0.5);
        if (face && faces) faces.push(face);
      }
    };

    const count = this.pool.templates.length;
    const end = count * (WIDTH + GAP) + 4;
    const line = [0, 0, 0, 0];
    context.strokeStyle = rgba(palette.muted, 0.08);
    context.lineWidth = 1;
    const ground = -PLINTH - 0.01;
    for (let x = -8; x <= end; x += 2)
      if (project(x, ground, -8, line, 0) && project(x, ground, DEPTH + 8, line, 2)) {
        context.beginPath();
        context.moveTo(line[0], line[1]);
        context.lineTo(line[2], line[3]);
        context.stroke();
      }
    for (let z = -8; z <= DEPTH + 8; z += 2)
      if (project(-8, ground, z, line, 0) && project(end, ground, z, line, 2)) {
        context.beginPath();
        context.moveTo(line[0], line[1]);
        context.lineTo(line[2], line[3]);
        context.stroke();
      }

    const platforms = Array.from({ length: count }, (_, index) => index).sort(
      (left, right) => Math.abs(templateCenter(right) - ex) - Math.abs(templateCenter(left) - ex)
    );
    for (const template of platforms) {
      const offset = template * (WIDTH + GAP);
      const current = template === this.template;
      prism(
        [offset - 0.4, offset + WIDTH + 0.4, -PLINTH, 0, -0.4, DEPTH + 0.4],
        [
          mixColor(palette.muted, palette.surface, current ? 16 : 9),
          mixColor(palette.muted, palette.surface, 4, 0.8),
          mixColor(palette.muted, palette.surface, 6, 0.9),
        ],
        current ? rgba(palette.accent, 1) : rgba(palette.muted, 0.2)
      );
    }

    // Painter's order: farthest box first.
    for (let index = 0; index < boxes.length; index += 1) {
      const box = boxes[index];
      const dx = (box.x0 + box.x1) / 2 - ex;
      const dy = heights[index] / 2 - ey;
      const dz = (box.z0 + box.z1) / 2 - ez;
      this.depths[index] = dx * dx + dy * dy + dz * dz;
    }
    const depths = this.depths;
    this.order.sort((left, right) => depths[right] - depths[left]);

    const seam = rgba(palette.surface, 0.55);
    this.hits = [];
    for (const index of this.order) {
      const box = boxes[index];
      const faces: Face[] = [];
      prism([box.x0, box.x1, 0, heights[index], box.z0, box.z1], [box.top, box.sideX, box.sideZ], seam, faces);
      this.hits.push({ box: index, faces });
    }

    const selected = this.pool.transactions.at(this.selected);
    if (selected) {
      const accent = rgba(palette.accent, 1);
      const text = rgba(palette.text, 1);
      for (const member of this.pool.chunks[selected.chunk].transactions) {
        const index = boxes.findIndex((box) => box.transaction === member);
        if (index < 0) continue;
        const [box, top] = [boxes[index], heights[index]];
        const mine = member === this.selected;
        fill(quad([box.x0, top, box.z0, box.x1, top, box.z0, box.x1, top, box.z1, box.x0, top, box.z1]), null, mine ? accent : text, mine ? 2.5 : 1.5);
      }
    }

    context.textAlign = 'center';
    context.textBaseline = 'top';
    this.labels.forEach((label, template) => {
      if (!project(templateCenter(template), -PLINTH, DEPTH + 1.1, line, 0)) return;
      context.font = `600 13px ${palette.font}`;
      context.fillStyle = rgba(template === this.template ? palette.accent : palette.muted, 1);
      context.fillText(label.title, line[0], line[1]);
      context.font = `12px ${palette.font}`;
      context.fillStyle = rgba(palette.muted, 1);
      context.fillText(label.detail, line[0], line[1] + 17);
    });

    const hovered = boxes.at(this.hover);
    if (hovered && this.pointer) {
      const transaction = this.pool.transactions[hovered.transaction];
      const lines = [
        `${formatTemplateRate(transactionRate(transaction))} sat/vB own · ${formatTemplateRate(chunkRate(this.pool.chunks[transaction.chunk]))} chunk`,
        `${formatInteger(transaction.size)} vB · ${formatInteger(transaction.fee)} sat`,
      ];
      context.font = `12px ${palette.font}`;
      context.textAlign = 'left';
      const boxWidth = Math.max(...lines.map((text) => context.measureText(text).width)) + 20;
      const x = Math.min(this.pointer[0] + 14, width - boxWidth - 6);
      const y = Math.min(this.pointer[1] + 14, height - 50);
      context.fillStyle = rgba(palette.surface, 0.95);
      context.strokeStyle = rgba(palette.accent, 0.6);
      context.lineWidth = 1;
      context.beginPath();
      context.rect(x, y, boxWidth, 42);
      context.fill();
      context.stroke();
      context.fillStyle = rgba(palette.text, 1);
      context.fillText(lines[0], x + 10, y + 7);
      context.fillStyle = rgba(palette.muted, 1);
      context.fillText(lines[1], x + 10, y + 23);
    }
  }

  // Topmost box under the point, or -1.
  private pick(x: number, y: number): number {
    for (let index = this.hits.length - 1; index >= 0; index -= 1)
      if (this.hits[index].faces.some((face) => inside(x, y, face))) return this.hits[index].box;
    return -1;
  }

  private local(event: PointerEvent): [number, number] {
    const bounds = this.canvas.getBoundingClientRect();
    return [event.clientX - bounds.left, event.clientY - bounds.top];
  }

  private readonly pointerDown = (event: PointerEvent): void => {
    this.canvas.setPointerCapture(event.pointerId);
    const [x, y] = this.local(event);
    this.drag = { x, y, startX: x, startY: y, pan: event.shiftKey || event.button === 2, moved: false };
    this.hover = -1;
    this.goal = null;
  };

  private readonly pointerMove = (event: PointerEvent): void => {
    const [x, y] = this.local(event);
    const drag = this.drag;
    if (!drag) {
      this.pointer = [x, y];
      const hover = this.pick(x, y);
      if (hover !== this.hover) {
        this.hover = hover;
        this.request();
      }
      return;
    }

    const [dx, dy] = [x - drag.x, y - drag.y];
    [drag.x, drag.y] = [x, y];
    if (Math.abs(x - drag.startX) + Math.abs(y - drag.startY) > 4) drag.moved = true;
    const camera = this.camera;
    if (drag.pan) {
      const scale = camera.distance * 0.0016;
      const [sin, cos] = [Math.sin(camera.azimuth), Math.cos(camera.azimuth)];
      camera.x += (-dx * cos - dy * sin) * scale;
      camera.z += (dx * sin - dy * cos) * scale;
    } else {
      camera.azimuth -= dx * 0.006;
      camera.elevation = Math.max(0.1, Math.min(1.45, camera.elevation + dy * 0.005));
    }
    this.request();
  };

  private readonly pointerUp = (event: PointerEvent): void => {
    const drag = this.drag;
    this.drag = null;
    if (!drag || drag.moved) return;
    const box = this.boxes.at(this.pick(...this.local(event)));
    if (box) this.onSelect(box.transaction);
  };

  private readonly pointerLeave = (): void => {
    this.pointer = null;
    if (this.hover < 0) return;
    this.hover = -1;
    this.request();
  };

  private readonly contextMenu = (event: MouseEvent): void => {
    event.preventDefault();
  };

  private readonly wheel = (event: WheelEvent): void => {
    event.preventDefault();
    this.zoom(Math.exp(event.deltaY * 0.0012));
  };

  private readonly keyDown = (event: KeyboardEvent): void => {
    const camera = this.camera;
    switch (event.key) {
      case 'ArrowLeft': {
        camera.azimuth += 0.08;
        break;
      }
      case 'ArrowRight': {
        camera.azimuth -= 0.08;
        break;
      }
      case 'ArrowUp': {
        camera.elevation = Math.min(1.45, camera.elevation + 0.06);
        break;
      }
      case 'ArrowDown': {
        camera.elevation = Math.max(0.1, camera.elevation - 0.06);
        break;
      }
      case '+':
      case '=': {
        this.zoom(0.9);
        break;
      }
      case '-': {
        this.zoom(1.1);
        break;
      }
      default: {
        return;
      }
    }
    event.preventDefault();
    this.request();
  };

  private zoom(factor: number): void {
    this.camera.distance = Math.max(6, Math.min(110, this.camera.distance * factor));
    this.request();
  }
}
