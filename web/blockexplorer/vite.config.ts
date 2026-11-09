import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";
import tsconfigPaths from "vite-tsconfig-paths";

export default defineConfig({
  plugins: [tailwindcss(), react(), tsconfigPaths()],
  build: {
    assetsDir: "", // all files in dist root
    rollupOptions: {
      output: {
        entryFileNames: `script.js`, // JS bundle name
        chunkFileNames: `[name].js`, // dynamic imports (if any)
        assetFileNames: (assetInfo) => {
          const names =
            assetInfo.names ?? (assetInfo.name ? [assetInfo.name] : []);
          if (names.some((name) => name.endsWith(".css"))) {
            return "style.css"; // CSS file name
          }
          return "[name].[ext]"; // other assets (e.g. images)
        },
      },
    },
  },
});
