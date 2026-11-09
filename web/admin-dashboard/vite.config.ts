import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";
import tsconfigPaths from "vite-tsconfig-paths";

export default defineConfig({
  plugins: [tailwindcss(), react(), tsconfigPaths()],
  build: {
    assetsDir: "",
    rollupOptions: {
      output: {
        entryFileNames: "script.js",
        chunkFileNames: "[name].js",
        assetFileNames: (assetInfo) => {
          const names =
            assetInfo.names ?? (assetInfo.name ? [assetInfo.name] : []);
          if (names.some((name) => name.endsWith(".css"))) {
            return "style.css";
          }
          return "[name].[ext]";
        },
      },
    },
  },
});
