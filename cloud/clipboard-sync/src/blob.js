import { put, get } from "@vercel/blob";

export function createBlobStorage(env = process.env) {
  // Connected private store, authenticated with Vercel's short-lived OIDC.
  const options = { access: "private", storeId: env.NT_STORE_ID || env.BLOB_STORE_ID };
  if (env.BLOB_READ_WRITE_TOKEN || env.NT_READ_WRITE_TOKEN) options.token = env.BLOB_READ_WRITE_TOKEN || env.NT_READ_WRITE_TOKEN;
  return {
    async check() {
      const result = await get("clipboard/.healthcheck/not-a-user-file.png", options);
      if (result?.stream) await result.stream.cancel();
    },
    async put(userId, hash, bytes) {
      return put(`clipboard/${userId}/${hash}.png`, bytes, {
        ...options, addRandomSuffix: false, allowOverwrite: true, contentType: "image/png", cacheControlMaxAge: 60
      });
    },
    async get(userId, hash) {
      const result = await get(`clipboard/${userId}/${hash}.png`, options);
      return result?.statusCode === 200 ? result.stream : null;
    }
  };
}
