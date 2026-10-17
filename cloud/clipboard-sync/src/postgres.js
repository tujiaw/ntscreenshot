import { readFileSync } from "node:fs";
import { Pool, neonConfig } from "@neondatabase/serverless";
import { attachDatabasePool } from "@vercel/functions";
import ws from "ws";

neonConfig.webSocketConstructor = ws;
const schema = readFileSync(new URL("../migrations/0001_sync.sql", import.meta.url), "utf8");

export class PostgresStore {
  constructor(pool) { this.pool = pool; this.initialization = null; }
  async initialize() {
    if (!this.initialization) {
      this.initialization = this.transaction(async db => {
        await db.query("SELECT pg_advisory_xact_lock(781945612)");
        for (const statement of schema.split(";").map(s => s.trim()).filter(Boolean)) await db.query(statement);
        // JSON preserves valid escaped clipboard control characters (including
        // U+0000), which PostgreSQL JSONB cannot represent. Upgrade the initial
        // deployment in place without dropping favorites or their cursors.
        const legacy = await db.query("SELECT table_name FROM information_schema.columns WHERE table_schema = current_schema() AND table_name IN ('clipboard_events', 'clipboard_favorites') AND column_name = 'record' AND data_type = 'jsonb'");
        for (const row of legacy.rows) {
          if (row.table_name === "clipboard_events") await db.query("ALTER TABLE clipboard_events ALTER COLUMN record TYPE JSON USING record::json");
          if (row.table_name === "clipboard_favorites") await db.query("ALTER TABLE clipboard_favorites ALTER COLUMN record TYPE JSON USING record::json");
        }
      }).catch(error => { this.initialization = null; throw error; });
    }
    await this.initialization;
  }
  query(text, values = []) { return this.pool.query(text, values); }
  async transaction(callback) {
    const client = await this.pool.connect();
    try {
      await client.query("BEGIN");
      const result = await callback(client);
      await client.query("COMMIT");
      return result;
    } catch (error) {
      await client.query("ROLLBACK").catch(() => {});
      throw error;
    } finally { client.release(); }
  }
  async forUser(userId, callback) {
    return this.transaction(async db => {
      // Serialize this user's writes, including revision allocation and commit.
      // A cursor cannot advance past another uncommitted operation for them.
      await db.query("SELECT pg_advisory_xact_lock(hashtextextended($1, 0))", [userId]);
      return callback(db);
    });
  }
}

let productionStore;
export function getStore(env = process.env) {
  if (!productionStore) {
    const connectionString = env.nt_DATABASE_URL || env.DATABASE_URL || env.nt_POSTGRES_URL || env.POSTGRES_URL;
    if (!connectionString) throw Object.assign(new Error("service_not_configured"), { status: 503 });
    const pool = new Pool({ connectionString, max: 3, connectionTimeoutMillis: 10000, idleTimeoutMillis: 10000, allowExitOnIdle: true });
    attachDatabasePool(pool);
    productionStore = new PostgresStore(pool);
  }
  return productionStore;
}
