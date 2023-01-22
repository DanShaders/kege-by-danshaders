import fs from "node:fs/promises";
import path from "node:path";
import { build, context } from 'esbuild';
import htmlPlugin from '@chialab/esbuild-plugin-html';

const type = process.argv[2];
const documentRoot = process.argv.length > 3 ? process.argv[3] : "/";

const production = type === "prod";
const addHashInProduction = production ? "-[hash]" : "";
const watch = type === "watch";

const outDirectory = "build/";

const options = {
  entryPoints: ["index.html"],
  format: "esm",
  bundle: true,
  outdir: outDirectory,
  entryNames: "[name]",
  chunkNames: "[name]" + addHashInProduction,
  assetNames: "assets/[name]" + addHashInProduction,
  minify: production,
  sourcemap: !production,
  splitting: true,
  loader: {
    ".svg": "dataurl",
    ".woff": "file",
    ".woff2": "file",
    ".ttf": "file",
    ".png": "file",
  },
  define: {
    "window.documentRoot": JSON.stringify(documentRoot),
  },
  plugins: [
    htmlPlugin({
      documentRoot: documentRoot,
    })
  ],
};

for (const file of await fs.readdir(outDirectory)) {
  await fs.rm(path.join(outDirectory, file), { recursive: true });
}

if (watch) {
  await (await context(options)).watch();
} else {
  await build(options);
}
