/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_ADMIN_API_URL?: string;
  readonly VITE_NATIVE_API_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
