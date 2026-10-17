import { isDeepStrictEqual } from "node:util";
import { ipAddress } from "@vercel/functions";
import { MAX_IMAGE, HASH, hash, secret, json, fail, readBytes, body, validateRecord } from "./protocol.js";

export function createService({ db, blobs, env = {}, upstream = fetch, clock = () => Date.now() }) {
  async function github(path, parameters) {
    const response = await upstream(`https://github.com/login/${path}`, {
      method: "POST", headers: { Accept: "application/json", "Content-Type": "application/x-www-form-urlencoded", "User-Agent": "ntscreenshot-sync" },
      body: new URLSearchParams(parameters), signal: AbortSignal.timeout(15000)
    });
    if (!response.ok) fail(502, "github_unavailable");
    return response.json();
  }
  const first = async (executor, sql, parameters = []) => (await executor.query(sql, parameters)).rows[0];
  const allowedUser = id => {
    const allowed = (env.ALLOWED_GITHUB_IDS || "").split(",").map(s => s.trim()).filter(Boolean);
    if (allowed.length && !allowed.includes(id)) fail(403, "account_not_allowed");
  };
  async function authenticate(request, now) {
    const match = /^Bearer ([a-f0-9]{64})$/.exec(request.headers.get("Authorization") || "");
    if (!match) fail(401, "sign_in_required");
    const tokenHash = hash(match[1]);
    const session = await first(db, "SELECT * FROM clipboard_sessions WHERE token_hash = $1 AND expires_at > $2", [tokenHash, now]);
    if (!session) fail(401, "session_expired");
    allowedUser(session.user_id);
    return { ...session, tokenHash };
  }
  return {
    async fetch(request) {
      try {
        const url = new URL(request.url);
        const path = url.searchParams.get("route") || url.pathname;
        const now = Math.floor(clock() / 1000);
        await db.initialize();
        if ((path === "/v1/health" || path === "/") && request.method === "GET") {
          if (url.searchParams.get("check") === "storage" && blobs.check) await blobs.check();
          return json({ service: "ntscreenshot-sync", protocol: 1, storage: "neon-private-blob", loginConfigured: Boolean(env.GITHUB_CLIENT_ID) });
        }
        if (path === "/v1/auth/start" && request.method === "POST") {
          if (!env.GITHUB_CLIENT_ID) fail(503, "service_not_configured");
          const ip = hash(ipAddress(request) || "unknown");
          const rate = await first(db,
            "INSERT INTO clipboard_auth_limits(ip_hash, window_start, attempts) VALUES ($1, $2, 1) " +
            "ON CONFLICT(ip_hash) DO UPDATE SET attempts = CASE WHEN clipboard_auth_limits.window_start <= $2 - 60 THEN 1 ELSE clipboard_auth_limits.attempts + 1 END, " +
            "window_start = CASE WHEN clipboard_auth_limits.window_start <= $2 - 60 THEN $2 ELSE clipboard_auth_limits.window_start END " +
            "WHERE clipboard_auth_limits.window_start <= $2 - 60 OR clipboard_auth_limits.attempts < 10 RETURNING attempts", [ip, now]);
          if (!rate) fail(429, "too_many_attempts");
          await db.query("DELETE FROM clipboard_auth_requests WHERE expires_at <= $1", [now]);
          await db.query("DELETE FROM clipboard_sessions WHERE expires_at <= $1", [now]);
          await db.query("DELETE FROM clipboard_auth_limits WHERE window_start < $1 - 3600", [now]);
          const active = await first(db, "SELECT COUNT(*) AS n FROM clipboard_auth_requests");
          if (Number(active.n) >= 1000) fail(429, "too_many_attempts");
          const grant = await github("device/code", { client_id: env.GITHUB_CLIENT_ID, scope: "" });
          if (!grant.device_code || !grant.user_code || grant.verification_uri !== "https://github.com/login/device") fail(502, "github_unavailable");
          const ticket = secret();
          const interval = Math.max(5, Number(grant.interval) || 5);
          const expiresIn = Math.min(900, Number(grant.expires_in) || 900);
          await db.query("INSERT INTO clipboard_auth_requests VALUES ($1, $2, $3, $4, $5)", [hash(ticket), grant.device_code, now + expiresIn, now + interval, interval]);
          return json({ ticket, userCode: grant.user_code, verificationUrl: grant.verification_uri, interval, expiresIn });
        }
        if (path === "/v1/auth/poll" && request.method === "POST") {
          const input = await body(request);
          if (!HASH.test(input?.ticket)) fail(400, "invalid_ticket");
          const ticketHash = hash(input.ticket);
          const pending = await first(db, "SELECT * FROM clipboard_auth_requests WHERE ticket_hash = $1 AND expires_at > $2", [ticketHash, now]);
          if (!pending) fail(410, "authorization_expired");
          const claimed = await first(db, "UPDATE clipboard_auth_requests SET next_poll = $1 WHERE ticket_hash = $2 AND next_poll <= $3 AND expires_at > $3 RETURNING ticket_hash", [now + pending.interval_seconds, ticketHash, now]);
          if (!claimed) return json({ pending: true, interval: pending.interval_seconds });
          const grant = await github("oauth/access_token", { client_id: env.GITHUB_CLIENT_ID, device_code: pending.device_code, grant_type: "urn:ietf:params:oauth:grant-type:device_code" });
          if (grant.error === "authorization_pending") return json({ pending: true, interval: pending.interval_seconds });
          if (grant.error === "slow_down") {
            const interval = pending.interval_seconds + 5;
            await db.query("UPDATE clipboard_auth_requests SET interval_seconds = $1, next_poll = $2 WHERE ticket_hash = $3", [interval, now + interval, ticketHash]);
            return json({ pending: true, interval });
          }
          if (grant.error || !grant.access_token) {
            await db.query("DELETE FROM clipboard_auth_requests WHERE ticket_hash = $1", [ticketHash]);
            fail(400, grant.error === "access_denied" ? "access_denied" : "authorization_expired");
          }
          const response = await upstream("https://api.github.com/user", {
            headers: { Authorization: `Bearer ${grant.access_token}`, Accept: "application/vnd.github+json", "User-Agent": "ntscreenshot-sync" }, signal: AbortSignal.timeout(15000)
          });
          if (!response.ok) fail(502, "github_unavailable");
          const user = await response.json();
          if (!Number.isSafeInteger(user.id) || !user.login) fail(502, "github_unavailable");
          const userId = String(user.id);
          allowedUser(userId);
          const token = secret();
          await db.transaction(async tx => {
            const consumed = await first(tx, "DELETE FROM clipboard_auth_requests WHERE ticket_hash = $1 AND expires_at > $2 RETURNING ticket_hash", [ticketHash, now]);
            if (!consumed) fail(410, "authorization_expired");
            await tx.query("INSERT INTO clipboard_accounts(user_id) VALUES ($1) ON CONFLICT DO NOTHING", [userId]);
            await tx.query("INSERT INTO clipboard_sessions VALUES ($1, $2, $3, $4)", [hash(token), userId, user.login, now + 90 * 86400]);
          });
          return json({ token, userId, login: user.login });
        }
        const session = await authenticate(request, now);
        const userId = session.user_id;
        if (path === "/v1/auth/logout" && request.method === "POST") {
          await db.query("DELETE FROM clipboard_sessions WHERE token_hash = $1", [session.tokenHash]);
          return json({ ok: true });
        }
        const imageRoute = /^\/v1\/images\/([a-f0-9]{64})$/.exec(path);
        if (imageRoute) {
          const imageHash = imageRoute[1];
          if (request.method === "PUT") {
            const bytes = await readBytes(request, MAX_IMAGE);
            if (bytes.length < 24 || !bytes.subarray(0, 8).equals(Buffer.from([137,80,78,71,13,10,26,10])) || hash(bytes) !== imageHash) fail(400, "invalid_image");
            const width = bytes.readUInt32BE(16), height = bytes.readUInt32BE(20);
            if (!width || !height || width * height > 72e6) fail(413, "image_too_large");
            const complete = await db.forUser(userId, async tx => {
              const existing = await first(tx, "SELECT complete FROM clipboard_images WHERE user_id = $1 AND hash = $2", [userId, imageHash]);
              if (existing) return existing.complete;
              const quota = await first(tx, "UPDATE clipboard_accounts SET image_bytes = image_bytes + $2 WHERE user_id = $1 AND image_bytes + $2 <= 104857600 RETURNING user_id", [userId, bytes.length]);
              if (!quota) fail(409, "storage_quota_exceeded");
              await tx.query("INSERT INTO clipboard_images(user_id, hash, size) VALUES ($1, $2, $3)", [userId, imageHash, bytes.length]);
              return false;
            });
            if (!complete) {
              await blobs.put(userId, imageHash, bytes);
              await db.query("UPDATE clipboard_images SET complete = TRUE WHERE user_id = $1 AND hash = $2", [userId, imageHash]);
            }
            return json({ ok: true });
          }
          if (request.method === "GET") {
            const existing = await first(db, "SELECT 1 FROM clipboard_images WHERE user_id = $1 AND hash = $2 AND complete", [userId, imageHash]);
            if (!existing) fail(404, "image_not_found");
            const image = await blobs.get(userId, imageHash);
            if (!image) fail(404, "image_not_found");
            return new Response(image, { headers: { "Content-Type": "image/png", "Cache-Control": "private, no-store", "X-Content-Type-Options": "nosniff" } });
          }
        }
        if (path === "/v1/sync" && request.method === "POST") {
          const input = await body(request);
          if (!input || !Number.isSafeInteger(input.cursor) || input.cursor < 0 || !Array.isArray(input.changes) || input.changes.length > 8) fail(400, "invalid_sync_request");
          const records = input.changes.map(validateRecord);
          if (new Set(records.map(r => r.operationId)).size !== records.length) fail(400, "duplicate_operation");
          const result = await db.forUser(userId, async tx => {
            const acknowledgements = [];
            for (const record of records) {
              const existing = await first(tx, "SELECT revision, record FROM clipboard_events WHERE user_id = $1 AND operation_id = $2", [userId, record.operationId]);
              if (existing) {
                if (!isDeepStrictEqual(existing.record, record)) fail(409, "operation_mismatch");
                acknowledgements.push({ id: record.id, operationId: record.operationId, revision: Number(existing.revision) });
                continue;
              }
              if (!record.deleted && record.kind === "image" && !(await first(tx, "SELECT 1 FROM clipboard_images WHERE user_id = $1 AND hash = $2 AND complete", [userId, record.imageHash]))) fail(409, "image_missing");
              const recordJson = JSON.stringify(record);
              const quota = await first(tx, "UPDATE clipboard_accounts SET event_bytes = event_bytes + $2, event_count = event_count + 1 WHERE user_id = $1 AND event_bytes + $2 <= 16777216 AND event_count < 50000 RETURNING user_id", [userId, Buffer.byteLength(recordJson)]);
              if (!quota) fail(409, "sync_quota_exceeded");
              const event = await first(tx, "INSERT INTO clipboard_events(user_id, operation_id, item_id, record) VALUES ($1, $2, $3, $4::json) RETURNING revision", [userId, record.operationId, record.id, recordJson]);
              await tx.query("INSERT INTO clipboard_favorites VALUES ($1, $2, $3, $4, $5::json) ON CONFLICT(user_id, item_id) DO UPDATE SET revision = EXCLUDED.revision, deleted = EXCLUDED.deleted, record = EXCLUDED.record", [userId, record.id, event.revision, record.deleted, recordJson]);
              acknowledgements.push({ id: record.id, operationId: record.operationId, revision: Number(event.revision) });
            }
            const page = await tx.query("SELECT revision, record FROM clipboard_events WHERE user_id = $1 AND revision > $2 ORDER BY revision LIMIT 21", [userId, input.cursor]);
            const rows = [];
            let responseBytes = 8192; // Leave room for acknowledgements and envelope.
            for (const row of page.rows.slice(0, 20)) {
              const size = Buffer.byteLength(JSON.stringify({ ...row.record, revision: Number(row.revision) }));
              if (rows.length && responseBytes + size > 3 * 1024 * 1024) break;
              rows.push(row);
              responseBytes += size;
            }
            return { acknowledgements, changes: rows.map(row => ({ ...row.record, revision: Number(row.revision) })), cursor: rows.length ? Number(rows.at(-1).revision) : input.cursor, more: page.rows.length > rows.length };
          });
          return json(result);
        }
        return json({ error: "not_found" }, 404);
      } catch (error) {
        return json({ error: error.status ? error.message : "service_unavailable" }, error.status || 503);
      }
    }
  };
}
