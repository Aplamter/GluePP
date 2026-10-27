// Rasterizes a Scratch project's SVG costumes to PNG the way Scratch does it
// (browser SVG renderer + Scratch's built-in fonts), copies bitmaps and sounds,
// and writes assets/manifest.json for the runtime.
// Usage: node rasterize.js <unzipped sb3 dir> <out assets dir>
const fs = require("fs");
const path = require("path");
const { chromium } = require("playwright");

const FONT_DIR = path.join(__dirname, "node_modules/scratch-render-fonts/src");
const FONTS = {
  "Sans Serif": "NotoSans-Medium.ttf",
  Serif: "SourceSerifPro-Regular.otf",
  Handwriting: "handlee-regular.ttf",
  Marker: "Knewave.ttf",
  Curly: "Griffy-Regular.ttf",
  Pixel: "Grand9K-Pixel.ttf",
  Scratch: "Scratch.ttf",
};
const fontCss = Object.entries(FONTS)
  .map(([name, file]) => {
    const b64 = fs.readFileSync(path.join(FONT_DIR, file)).toString("base64");
    return `@font-face{font-family:"${name}";src:url("data:font/ttf;base64,${b64}");}`;
  })
  .join("");

const safe = (s) => s.replace(/[^A-Za-z0-9._-]/g, (c) => "_x" + c.codePointAt(0).toString(16) + "_");

function prepareSvg(text) {
  // Same fix-ups Scratch applies before drawing: namespace, embedded fonts.
  if (!/<svg[^>]*xmlns=/.test(text)) text = text.replace("<svg ", '<svg xmlns="http://www.w3.org/2000/svg" ');
  const open = text.match(/<svg[^>]*>/)[0];
  const vb = (open.match(/viewBox="([^"]+)"/) || [])[1];
  let x = 0, y = 0, w, h;
  if (vb) [x, y, w, h] = vb.split(/[\s,]+/).map(Number);
  else {
    w = parseFloat((open.match(/width="([^"]+)"/) || [])[1]);
    h = parseFloat((open.match(/height="([^"]+)"/) || [])[1]);
  }
  // Render exactly the viewBox area at its own size (what Scratch's SVGSkin measures).
  let fixed = open.replace(/\s(width|height)="[^"]*"/g, "");
  fixed = fixed.replace("<svg ", `<svg width="${w}" height="${h}" `);
  if (!vb) fixed = fixed.replace("<svg ", `<svg viewBox="0 0 ${w} ${h}" `);
  text = text.replace(open, fixed + `<defs><style>${fontCss}</style></defs>`);
  return { text, x, y, w, h };
}

(async () => {
  const [src, out] = process.argv.slice(2);
  const project = JSON.parse(fs.readFileSync(path.join(src, "project.json"), "utf8"));
  fs.mkdirSync(path.join(out, "costumes"), { recursive: true });
  fs.mkdirSync(path.join(out, "sounds"), { recursive: true });

  const browser = await chromium.launch({ ...(process.env.CHROMIUM_PATH ? { executablePath: process.env.CHROMIUM_PATH } : {}) });
  const page = await browser.newPage();
  await page.setContent("<canvas id=c></canvas>");

  const manifest = { targets: [] };
  for (const [ti, t] of project.targets.entries()) {
    const entry = { name: t.name, costumes: [], sounds: [] };
    for (const [ci, c] of t.costumes.entries()) {
      const base = `${ti}_${String(ci + 1).padStart(3, "0")}_${safe(c.name)}`;
      const file = `costumes/${base}.png`;
      const data = fs.readFileSync(path.join(src, c.md5ext));
      let info;
      if (c.dataFormat === "svg") {
        const svg = prepareSvg(data.toString("utf8"));
        const big = Math.max(svg.w, svg.h);
        const scale = big < 64 ? 8 : big > 1024 ? Math.max(1, 4096 / big) : 4;
        const png = await page.evaluate(
          async ({ text, w, h, scale }) => {
            const img = new Image();
            img.src = "data:image/svg+xml;base64," + btoa(unescape(encodeURIComponent(text)));
            await img.decode().catch(() => {});
            const c = document.getElementById("c");
            c.width = Math.max(1, Math.ceil(w * scale));
            c.height = Math.max(1, Math.ceil(h * scale));
            const g = c.getContext("2d");
            g.clearRect(0, 0, c.width, c.height);
            if (img.naturalWidth) g.drawImage(img, 0, 0, w * scale, h * scale);
            return c.toDataURL("image/png").split(",")[1];
          },
          { text: svg.text, w: svg.w, h: svg.h, scale }
        );
        fs.writeFileSync(path.join(out, file), Buffer.from(png, "base64"));
        info = {
          scale, // texture pixels per stage unit
          width: svg.w,
          height: svg.h,
          centerX: c.rotationCenterX - svg.x,
          centerY: c.rotationCenterY - svg.y,
        };
      } else {
        fs.writeFileSync(path.join(out, file), data);
        const res = c.bitmapResolution || 1;
        const w = data.readUInt32BE(16), h = data.readUInt32BE(20); // PNG IHDR
        info = { scale: res, width: w / res, height: h / res, centerX: c.rotationCenterX / res, centerY: c.rotationCenterY / res };
      }
      entry.costumes.push({ name: c.name, file, ...info });
    }
    for (const [si, s] of t.sounds.entries()) {
      const file = `sounds/${ti}_${String(si + 1).padStart(3, "0")}_${safe(s.name)}.${s.dataFormat}`;
      fs.copyFileSync(path.join(src, s.md5ext), path.join(out, file));
      entry.sounds.push({ name: s.name, file });
    }
    manifest.targets.push(entry);
    console.log(t.name, entry.costumes.length, "costumes", entry.sounds.length, "sounds");
  }
  fs.writeFileSync(path.join(out, "manifest.json"), JSON.stringify(manifest, null, 1));
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
