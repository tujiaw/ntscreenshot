-- Namespaced tables share the existing Neon database without touching other apps.
CREATE TABLE IF NOT EXISTS clipboard_accounts (
  user_id TEXT PRIMARY KEY,
  image_bytes BIGINT NOT NULL DEFAULT 0,
  event_bytes BIGINT NOT NULL DEFAULT 0,
  event_count INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS clipboard_auth_requests (
  ticket_hash TEXT PRIMARY KEY,
  device_code TEXT NOT NULL,
  expires_at BIGINT NOT NULL,
  next_poll BIGINT NOT NULL,
  interval_seconds INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS clipboard_auth_expiry ON clipboard_auth_requests(expires_at);
CREATE TABLE IF NOT EXISTS clipboard_auth_limits (
  ip_hash TEXT PRIMARY KEY,
  window_start BIGINT NOT NULL,
  attempts INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS clipboard_sessions (
  token_hash TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES clipboard_accounts(user_id),
  login TEXT NOT NULL,
  expires_at BIGINT NOT NULL
);
CREATE INDEX IF NOT EXISTS clipboard_session_expiry ON clipboard_sessions(expires_at);
CREATE TABLE IF NOT EXISTS clipboard_events (
  revision BIGSERIAL PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES clipboard_accounts(user_id),
  operation_id TEXT NOT NULL,
  item_id TEXT NOT NULL,
  record JSON NOT NULL,
  UNIQUE(user_id, operation_id)
);
CREATE INDEX IF NOT EXISTS clipboard_events_user_revision ON clipboard_events(user_id, revision);
CREATE TABLE IF NOT EXISTS clipboard_favorites (
  user_id TEXT NOT NULL REFERENCES clipboard_accounts(user_id),
  item_id TEXT NOT NULL,
  revision BIGINT NOT NULL,
  deleted BOOLEAN NOT NULL,
  record JSON NOT NULL,
  PRIMARY KEY(user_id, item_id)
);
CREATE TABLE IF NOT EXISTS clipboard_images (
  user_id TEXT NOT NULL REFERENCES clipboard_accounts(user_id),
  hash TEXT NOT NULL,
  size INTEGER NOT NULL,
  complete BOOLEAN NOT NULL DEFAULT FALSE,
  PRIMARY KEY(user_id, hash)
);
