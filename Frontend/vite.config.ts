import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  base: './',
  build: {
    outDir: '../Resources/dist', // JUCEのResourcesフォルダへ出力
    emptyOutDir: true
  }
})