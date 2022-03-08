const PROD = "prod";
const WATCH = "watch";

const type = process.argv[2];

require("esbuild")
  .build({
    entryPoints: ["src/app.ts", "css/main.css"],
    bundle: true,
    outdir: "build/",
    minify: type === PROD,
    sourcemap: type !== PROD,
    watch: type === WATCH,
  })
  .catch(() => process.exit(1));
