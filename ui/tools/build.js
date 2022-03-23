const fs = require("fs/promises");

const PROD = "prod";
const WATCH = "watch";

const type = process.argv[2];

require("esbuild")
  .build({
    entryPoints: ["src/app.ts", "src/admin.ts", "css/main.css"],
    format: "esm",
    bundle: true,
    outdir: "build/",
    entryNames: "[name]",
    chunkNames: "[name]",
    assetNames: "assets/[name]",
    minify: type === PROD,
    sourcemap: type !== PROD,
    watch: type === WATCH,
    splitting: true,
    loader: {
      ".svg": "dataurl",
      ".woff": "file",
      ".woff2": "file",
      ".ttf": "file",
    },
  })
  .catch(() => process.exit(1));
