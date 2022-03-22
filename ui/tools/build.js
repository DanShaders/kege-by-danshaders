const fs = require("fs/promises");

const PROD = "prod";
const WATCH = "watch";

const type = process.argv[2];

const svgCustomLoader = {
  name: "svg-custom-loader",
  // eslint-disable-next-line @typescript-eslint/explicit-function-return-type
  setup(build) {
    build.onLoad({ filter: /.*\.svg/ }, async (args) => {
      const loader = args.suffix.match(/loader=([a-zA-Z\-0-9]+)/);
      return {
        contents: await fs.readFile(args.path),
        loader: loader ? loader[1] : "text",
      };
    });
  },
};

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
    plugins: [svgCustomLoader],
    loader: {
      ".svg": "dataurl",
      ".woff": "file",
      ".woff2": "file",
      ".ttf": "file",
    },
  })
  .catch(() => process.exit(1));
