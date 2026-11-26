import { defineConfig } from '@playwright/test';
export default defineConfig({
  testDir: './browser', timeout: 700000, workers: 1,
  use: { baseURL: 'http://127.0.0.1:5173', headless: true },
  projects: [{ name: 'chromium', use: { browserName: 'chromium', launchOptions: { args: ['--autoplay-policy=no-user-gesture-required'] } } }, { name: 'firefox', use: { browserName: 'firefox' } }],
  webServer: { env: { M71_QUALIFY: '1' }, command: 'npm run dev -- --host 127.0.0.1', url: 'http://127.0.0.1:5173', reuseExistingServer: true },
});
