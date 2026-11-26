import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: { hmr: process.env.M71_QUALIFY === '1' ? false : undefined },
  base: process.env.VITE_BASE_PATH || './',
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    sourcemap: false,
    rollupOptions: {
      external: ['module', 'fs', 'path', 'url'],
    },
  },
});
