import { createService } from "../src/service.js";
import { getStore } from "../src/postgres.js";
import { createBlobStorage } from "../src/blob.js";

export default {
  async fetch(request) {
    try {
      return await createService({ db: getStore(), blobs: createBlobStorage(), env: process.env }).fetch(request);
    } catch {
      return Response.json({ error: "service_not_configured" }, { status: 503, headers: { "Cache-Control": "no-store" } });
    }
  }
};
