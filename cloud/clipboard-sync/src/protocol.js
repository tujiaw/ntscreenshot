import { createHash, randomBytes } from "node:crypto";

export const MAX_IMAGE = 2 * 1024 * 1024;
export const HASH = /^[a-f0-9]{64}$/;
const OP_ID = /^[a-zA-Z0-9-]{16,80}$/;
export const hash = value => createHash("sha256").update(value).digest("hex");
export const secret = () => randomBytes(32).toString("hex");
export function json(body, status = 200) { return Response.json(body, { status, headers: { "Cache-Control": "no-store" } }); }
export function fail(status, code) { throw Object.assign(new Error(code), { status }); }
export async function readBytes(request, limit = 2 * 1024 * 1024) {
  if (Number(request.headers.get("Content-Length")) > limit) fail(413, "payload_too_large");
  if (!request.body) return new Uint8Array();
  const reader = request.body.getReader();
  const chunks = [];
  let size = 0;
  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    size += value.length;
    if (size > limit) { await reader.cancel(); fail(413, "payload_too_large"); }
    chunks.push(value);
  }
  return Buffer.concat(chunks, size);
}
export async function body(request) {
  try { return JSON.parse(new TextDecoder().decode(await readBytes(request))); }
  catch (error) { if (error.status) throw error; fail(400, "invalid_json"); }
}
export function validateRecord(value) {
  if (!value || !HASH.test(value.id) || !OP_ID.test(value.operationId) || typeof value.deleted !== "boolean") fail(400, "invalid_record");
  const record = { id: value.id, operationId: value.operationId, deleted: value.deleted };
  if (record.deleted) return record;
  if (!Number.isSafeInteger(value.capturedAt) || value.capturedAt < 0) fail(400, "invalid_record");
  record.capturedAt = value.capturedAt;
  record.kind = value.kind;
  if (value.kind === "text") {
    if (typeof value.text !== "string" || !value.text || Buffer.byteLength(value.text) > 100 * 1024) fail(400, "text_too_large");
    if (hash(`text:${value.text}`) !== value.id) fail(400, "content_mismatch");
    record.text = value.text;
  } else if (value.kind === "image") {
    if (!HASH.test(value.imageHash) || value.id !== hash(`image:${value.imageHash}`) ||
        !Number.isInteger(value.width) || !Number.isInteger(value.height) || value.width < 1 || value.height < 1 || value.width * value.height > 72e6) fail(400, "invalid_image");
    Object.assign(record, { imageHash: value.imageHash, width: value.width, height: value.height });
  } else fail(400, "invalid_record");
  return record;
}
