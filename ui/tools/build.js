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
    chunkNames: "[name]-[hash]",
    minify: type === PROD,
    sourcemap: type !== PROD,
    watch: type === WATCH,
    splitting: true,
    loader: {
      ".svg": "dataurl",
    },
  })
  .catch(() => process.exit(1));
