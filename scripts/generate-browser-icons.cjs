// Optional asset regeneration tool: requires Node.js and sharp, not application dependencies.
const fs = require('node:fs/promises');
const path = require('node:path');
const sharp = require('sharp');

const icons = {
  folder: 'folder-open', home: 'house', refresh: 'refresh-cw', sidebar: 'panel-left',
  previous: 'chevron-left', next: 'chevron-right', fit: 'expand', actual: 'scan',
  crop: 'crop', 'rotate-left': 'rotate-ccw-square', 'rotate-right': 'rotate-cw-square',
  'flip-horizontal': 'flip-horizontal', 'flip-vertical': 'flip-vertical',
  resize: 'scaling', undo: 'undo-2', redo: 'redo-2', reset: 'history', play: 'play', pause: 'pause', frame: 'film',
  convert: 'replace', copy: 'copy', save: 'save', more: 'ellipsis', apply: 'check', cancel: 'x',
};
const base = 'https://raw.githubusercontent.com/lucide-icons/lucide/0.468.0/';
async function download(relative) {
  const response = await fetch(base + relative);
  if (!response.ok) throw new Error(`${relative}: ${response.status}`);
  return Buffer.from(await response.arrayBuffer());
}
async function main() {
  const root = path.join(__dirname, '..', 'src', 'resource');
  for (const [name, upstream] of Object.entries(icons)) {
    const svg = await download(`icons/${upstream}.svg`);
    await sharp(svg).resize(64, 64).png().toFile(path.join(root, 'images', `browser-${name}.png`));
  }
  await fs.mkdir(path.join(root, 'licenses'), { recursive: true });
  await fs.writeFile(path.join(root, 'licenses', 'lucide-LICENSE'), await download('LICENSE'));
}
main().catch(error => { console.error(error); process.exitCode = 1; });
