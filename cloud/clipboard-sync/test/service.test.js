import test from "node:test";
import assert from "node:assert/strict";
import { randomUUID } from "node:crypto";
import { PGlite } from "@electric-sql/pglite";
import { PostgresStore } from "../src/postgres.js";
import { createService } from "../src/service.js";
import { hash } from "../src/protocol.js";

async function fixture() {
  const pg = new PGlite();
  const query = (text, values) => pg.query(text, values);
  const store = new PostgresStore({ query, connect: async () => ({ query, release() {} }) });
  await store.initialize();
  const tokens = { a: "a".repeat(64), b: "b".repeat(64), other: "c".repeat(64) };
  for (const id of ["1", "2"]) await query("INSERT INTO clipboard_accounts(user_id) VALUES ($1)", [id]);
  for (const [token, user] of [[tokens.a, "1"], [tokens.b, "1"], [tokens.other, "2"]]) {
    await query("INSERT INTO clipboard_sessions VALUES ($1, $2, $3, $4)", [hash(token), user, `user-${user}`, Math.floor(Date.now() / 1000) + 3600]);
  }
  const files = new Map();
  const env = { GITHUB_CLIENT_ID: "test-client" };
  let mode = "approved";
  const upstream = async (url, options) => {
    if (url.endsWith("/device/code")) {
      assert.equal(new URLSearchParams(options.body).get("scope"), "");
      return Response.json({ device_code: "device-secret", user_code: "ABCD-EFGH", verification_uri: "https://github.com/login/device", interval: 5, expires_in: 900 });
    }
    if (url.endsWith("/oauth/access_token")) {
      return mode === "approved" ? Response.json({ access_token: "github-access-token-not-persisted" }) : Response.json({ error: mode });
    }
    if (url === "https://api.github.com/user") return Response.json({ id: 42, login: "tester" });
    throw new Error("Unexpected upstream");
  };
  const service = createService({ db: store, env, upstream, blobs: {
    async put(userId, imageHash, bytes) { files.set(`${userId}/${imageHash}`, new Uint8Array(bytes)); },
    async get(userId, imageHash) { return files.get(`${userId}/${imageHash}`) || null; }
  } });
  const request = (path, input = {}, token = tokens.a, method = "POST") => service.fetch(new Request(`https://sync.test${path}`, {
    method, headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json", "x-real-ip": "192.0.2.1" },
    ...(method === "GET" ? {} : { body: input instanceof Uint8Array ? input : JSON.stringify(input) })
  }));
  return { pg, query, store, request, tokens, files, env, setMode(value) { mode = value; } };
}
const text = (value, deleted = false) => ({ id: hash(`text:${value}`), operationId: randomUUID(), deleted, ...(deleted ? {} : { kind: "text", text: value, capturedAt: Date.now() }) });
const png = () => new Uint8Array(Buffer.from("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aD1sAAAAASUVORK5CYII=", "base64"));

test("Postgres schema is idempotent and health reports the shared service", async () => {
  const f = await fixture();
  try {
    const initial = await (await f.request("/v1/sync", { cursor: 0, changes: [text("migration survives")] })).json();
    await f.query("ALTER TABLE clipboard_events ALTER COLUMN record TYPE JSONB USING record::jsonb");
    await f.query("ALTER TABLE clipboard_favorites ALTER COLUMN record TYPE JSONB USING record::jsonb");
    await new PostgresStore({ query: f.query, connect: async () => ({ query: f.query, release() {} }) }).initialize();
    const preserved = await (await f.request("/v1/sync", { cursor: 0, changes: [] })).json();
    assert.equal(preserved.cursor, initial.cursor);
    assert.equal(preserved.changes[0].text, "migration survives");
    const response = await f.request("/v1/health", null, undefined, "GET");
    assert.equal(response.status, 200);
    assert.equal((await response.json()).storage, "neon-private-blob");
    const rewritten = await f.request("/api/sync?route=/v1/health", null, undefined, "GET");
    assert.equal(rewritten.status, 200);
  } finally { await f.pg.close(); }
});

test("two devices sync additions and deletes; lost-response retries never resurrect data", async () => {
  const f = await fixture();
  try {
    const addition = text("favorite");
    const first = await (await f.request("/v1/sync", { cursor: 0, changes: [addition] })).json();
    assert.equal(first.changes[0].text, "favorite");
    const deviceB = await (await f.request("/v1/sync", { cursor: 0, changes: [] }, f.tokens.b)).json();
    assert.equal(deviceB.changes[0].id, addition.id);
    const removed = await (await f.request("/v1/sync", { cursor: first.cursor, changes: [text("favorite", true)] }, f.tokens.b)).json();
    assert.equal(removed.changes[0].deleted, true);
    const retry = await (await f.request("/v1/sync", { cursor: removed.cursor, changes: [addition] })).json();
    assert.equal(retry.changes.length, 0);
    assert.equal(retry.acknowledgements[0].revision, first.cursor);
    assert.equal((await f.query("SELECT deleted FROM clipboard_favorites")).rows[0].deleted, true);
  } finally { await f.pg.close(); }
});

test("one database and Blob store still isolate accounts and revoke sessions", async () => {
  const f = await fixture();
  try {
    await f.request("/v1/sync", { cursor: 0, changes: [text("private")] });
    assert.deepEqual((await (await f.request("/v1/sync", { cursor: 0, changes: [] }, f.tokens.other)).json()).changes, []);
    const bytes = png(), imageHash = hash(bytes);
    assert.equal((await f.request(`/v1/images/${imageHash}`, bytes, undefined, "PUT")).status, 200);
    const downloaded = await f.request(`/v1/images/${imageHash}`, null, undefined, "GET");
    assert.deepEqual(new Uint8Array(await downloaded.arrayBuffer()), bytes);
    assert.equal((await f.request(`/v1/images/${imageHash}`, null, f.tokens.other, "GET")).status, 404);
    assert.ok(f.files.has(`1/${imageHash}`));
    assert.equal((await f.request("/v1/auth/logout")).status, 200);
    assert.equal((await f.request("/v1/sync", { cursor: 0, changes: [] })).status, 401);
  } finally { await f.pg.close(); }
});

test("pagination delivers every event without advancing over missing revisions", async () => {
  const f = await fixture();
  try {
    for (let offset = 0; offset < 48; offset += 8) {
      assert.equal((await f.request("/v1/sync", { cursor: 0, changes: Array.from({ length: 8 }, (_, i) => text(`item-${offset + i}`)) })).status, 200);
    }
    const received = [];
    let cursor = 0, more = true;
    while (more) {
      const page = await (await f.request("/v1/sync", { cursor, changes: [] })).json();
      received.push(...page.changes);
      cursor = page.cursor;
      more = page.more;
    }
    assert.equal(received.length, 48);
    assert.equal(new Set(received.map(r => r.id)).size, 48);
  } finally { await f.pg.close(); }
});

test("sync transactions roll back the entire batch on reused operation IDs", async () => {
  const f = await fixture();
  try {
    const existing = text("existing");
    await f.request("/v1/sync", { cursor: 0, changes: [existing] });
    const conflict = { id: existing.id, operationId: existing.operationId, deleted: true };
    const response = await f.request("/v1/sync", { cursor: 0, changes: [text("must roll back"), conflict] });
    assert.equal(response.status, 409);
    assert.equal(Number((await f.query("SELECT COUNT(*) AS n FROM clipboard_events")).rows[0].n), 1);
    assert.equal((await f.request("/v1/sync", { cursor: 0, changes: [text("x".repeat(102401))] })).status, 400);
    assert.equal((await f.request("/v1/sync", { cursor: -1, changes: [] })).status, 400);
  } finally { await f.pg.close(); }
});

test("JSON escaping cannot exceed the Vercel response limit or lose paginated events", async () => {
  const f = await fixture();
  try {
    for (let i = 0; i < 8; ++i) {
      assert.equal((await f.request("/v1/sync", { cursor: 0, changes: [text("\0".repeat(102398) + i)] })).status, 200);
    }
    let cursor = 0, more = true, count = 0;
    while (more) {
      const response = await f.request("/v1/sync", { cursor, changes: [] });
      const raw = await response.text();
      assert.ok(Buffer.byteLength(raw) < 3 * 1024 * 1024);
      const page = JSON.parse(raw);
      count += page.changes.length;
      cursor = page.cursor;
      more = page.more;
    }
    assert.equal(count, 8);
  } finally { await f.pg.close(); }
});

test("public-service quotas reserve images once and isolate different users", async () => {
  const f = await fixture();
  try {
    const bytes = png(), imageHash = hash(bytes);
    await f.request(`/v1/images/${imageHash}`, bytes, undefined, "PUT");
    await f.request(`/v1/images/${imageHash}`, bytes, undefined, "PUT");
    assert.equal(Number((await f.query("SELECT image_bytes FROM clipboard_accounts WHERE user_id = '1'")).rows[0].image_bytes), bytes.length);
    await f.query("UPDATE clipboard_accounts SET image_bytes = 104857600 WHERE user_id = '2'");
    assert.equal((await f.request(`/v1/images/${imageHash}`, bytes, f.tokens.other, "PUT")).status, 409);
    const big = new Uint8Array(2097153);
    assert.equal((await f.request(`/v1/images/${hash(big)}`, big, undefined, "PUT")).status, 413);
    await f.query("UPDATE clipboard_accounts SET event_bytes = 16777216 WHERE user_id = '1'");
    const quota = await f.request("/v1/sync", { cursor: 0, changes: [text("over quota")] });
    assert.equal((await quota.json()).error, "sync_quota_exceeded");
  } finally { await f.pg.close(); }
});

test("GitHub login creates an account and consumes the ticket exactly once", async () => {
  const f = await fixture();
  try {
    const start = await (await f.request("/v1/auth/start")).json();
    assert.equal(start.userCode, "ABCD-EFGH");
    assert.equal((await (await f.request("/v1/auth/poll", { ticket: start.ticket })).json()).pending, true);
    await f.query("UPDATE clipboard_auth_requests SET next_poll = 0");
    const login = await (await f.request("/v1/auth/poll", { ticket: start.ticket })).json();
    assert.equal(login.userId, "42");
    assert.match(login.token, /^[a-f0-9]{64}$/);
    assert.equal((await f.request("/v1/sync", { cursor: 0, changes: [] }, login.token)).status, 200);
    assert.equal((await f.request("/v1/auth/poll", { ticket: start.ticket })).status, 410);
    assert.equal((await f.query("SELECT token_hash FROM clipboard_sessions WHERE user_id = '42'")).rows[0].token_hash, hash(login.token));
    for (let i = 0; i < 9; ++i) assert.equal((await f.request("/v1/auth/start")).status, 200);
    assert.equal((await f.request("/v1/auth/start")).status, 429);
  } finally { await f.pg.close(); }
});

test("denied or expired GitHub authorizations do not create a session", async () => {
  const f = await fixture();
  try {
    f.setMode("access_denied");
    const start = await (await f.request("/v1/auth/start")).json();
    await f.query("UPDATE clipboard_auth_requests SET next_poll = 0");
    assert.equal((await (await f.request("/v1/auth/poll", { ticket: start.ticket })).json()).error, "access_denied");
    assert.equal((await f.request("/v1/auth/poll", { ticket: start.ticket })).status, 410);
    assert.equal(Number((await f.query("SELECT COUNT(*) AS n FROM clipboard_sessions WHERE user_id = '42'")).rows[0].n), 0);
  } finally { await f.pg.close(); }
});
