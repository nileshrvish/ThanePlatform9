// Calls MCP tools on the unreal-engine bridge over stdio.
// Usage: node mcp_call.mjs <index.js> <outDir> '<json array of [toolName, args]>'
// Each result is written to <outDir>/<n>_<toolName>.txt; a short summary prints to stdout.
import { spawn } from "node:child_process";
import { writeFileSync, mkdirSync, readFileSync, existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

// Third argument is a path to a JSON file holding the calls array.
const [, , indexJs, outDir, callsFile] = process.argv;
const calls = JSON.parse(readFileSync(callsFile, "utf8").replace(/^﻿/, ""));
mkdirSync(outDir, { recursive: true });

// --- Simulated-touch lifecycle -------------------------------------------------------------
// Every "p9.Test.Touch Start" must be matched by an End, or Slate keeps that pointer "down" across
// PIE sessions and logs "touch starting ... without finding out it ever ended" on the next start.
// Active pointers (with their last position) are persisted next to this script so a pointer left down
// when PIE ended unexpectedly is released at the start of the next PIE session.
// MCP_HARNESS_LEGACY=1 disables all of this (control runs only).
const LEGACY = process.env.MCP_HARNESS_LEGACY === "1";
const TOUCH_STATE_FILE = join(dirname(fileURLToPath(import.meta.url)), "active_touches.json");
const activeTouches = new Map(
  !LEGACY && existsSync(TOUCH_STATE_FILE) ? JSON.parse(readFileSync(TOUCH_STATE_FILE, "utf8") || "[]") : []
);
let staleTouches = activeTouches.size > 0; // left over from an earlier run: release once PIE is confirmed playing

function saveTouches() {
  if (!LEGACY) writeFileSync(TOUCH_STATE_FILE, JSON.stringify([...activeTouches]));
}

function trackTouchCommand(command) {
  let m = command.match(/^p9\.Test\.Touch\s+(Start|Move|End)\s+(\d+)\s+([\d.]+)\s+([\d.]+)/i);
  if (m) {
    const pointer = Number(m[2]);
    if (/^end$/i.test(m[1])) activeTouches.delete(pointer);
    else activeTouches.set(pointer, [m[3], m[4]]);
    saveTouches();
    return;
  }
  m = command.match(/^p9\.Test\.TouchDrag\s+(\d+)\s+([\d.]+)\s+([\d.]+)/i);
  if (m && activeTouches.has(Number(m[1]))) {
    activeTouches.set(Number(m[1]), [m[2], m[3]]);
    saveTouches();
  }
}

async function isPiePlaying() {
  try {
    const status = await request("tools/call", { name: "play_in_editor", arguments: { operation: "status" } }, 10000);
    return /"is_playing":\s*true/.test(status.result?.content?.map((c) => c.text).join("\n") ?? "");
  } catch {
    return false;
  }
}

// Sends a touch-end for every active pointer while PIE is playing. If PIE is not playing the pointers are
// kept (persisted) and released at the start of the next PIE session instead.
async function releaseActiveTouches(reason) {
  if (LEGACY || activeTouches.size === 0) return;
  if (!(await isPiePlaying())) {
    console.log(`[harness] ${reason}: PIE not playing; ${activeTouches.size} pointer(s) kept for release in the next PIE session`);
    return;
  }
  for (const [pointer, [x, y]] of [...activeTouches]) {
    try {
      await request("tools/call", { name: "execute_console_command", arguments: { command: `p9.Test.Touch End ${pointer} ${x} ${y}` } }, 10000);
      activeTouches.delete(pointer);
      console.log(`[harness] ${reason}: released pointer ${pointer} at ${x} ${y}`);
    } catch (e) {
      console.log(`[harness] ${reason}: failed to release pointer ${pointer}: ${e.message}`);
    }
  }
  saveTouches();
  staleTouches = false;
  await new Promise((r) => setTimeout(r, 150)); // let the touch-ends process before anything else
}

const child = spawn("node", [indexJs], { stdio: ["pipe", "pipe", "ignore"] });
let buf = "";
const pending = new Map();
let nextId = 1;

child.stdout.on("data", (d) => {
  buf += d;
  let i;
  while ((i = buf.indexOf("\n")) >= 0) {
    const line = buf.slice(0, i).trim();
    buf = buf.slice(i + 1);
    if (!line) continue;
    const msg = JSON.parse(line);
    if (msg.id && pending.has(msg.id)) {
      pending.get(msg.id)(msg);
      pending.delete(msg.id);
    }
  }
});

function request(method, params = {}, timeoutMs = 60000) {
  const id = nextId++;
  child.stdin.write(JSON.stringify({ jsonrpc: "2.0", id, method, params }) + "\n");
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`timeout: ${method}`)), timeoutMs);
    pending.set(id, (value) => { clearTimeout(timer); resolve(value); });
  });
}

try {
  await request("initialize", {
    protocolVersion: "2024-11-05",
    capabilities: {},
    clientInfo: { name: "probe", version: "1.0" },
  });
  child.stdin.write(JSON.stringify({ jsonrpc: "2.0", method: "notifications/initialized" }) + "\n");

  const labelToName = new Map(JSON.parse(process.env.MCP_LABELS || "[]"));
  let n = 0;
  let lastLabel = "(none)";
  for (const [name, args] of calls) {
    n++;
    if (name === "__sleep") {
      await new Promise((r) => setTimeout(r, args.ms));
      continue;
    }
    if (name === "__label") {
      console.log(`### ${new Date().toISOString()} BEGIN ${args.text}`);
      lastLabel = args.text;
      continue;
    }
    if (name === "__assert_pie") {
      const status = await request("tools/call", { name: "play_in_editor", arguments: { operation: "status" } });
      const statusText = status.result?.content?.map((c) => c.text).join("\n") ?? "";
      if (!/"is_playing":\s*true/.test(statusText)) {
        console.log(`### ${new Date().toISOString()} ABORT: PIE not playing before "${args.before}". Last started test: "${lastLabel}". Status: ${statusText.replace(/\s+/g, " ")}`);
        process.exitCode = 3;
        break;
      }
      if (staleTouches) await releaseActiveTouches("stale pointers from an earlier session");
      continue;
    }
    // Stops PIE without releasing simulated touches (control runs only).
    if (name === "__raw_stop") {
      await request("tools/call", { name: "play_in_editor", arguments: { operation: "stop" } });
      console.log(`[harness] raw stop: ${activeTouches.size} pointer(s) left down`);
      continue;
    }
    if (name === "play_in_editor" && args.operation === "stop") {
      await releaseActiveTouches("before PIE stop");
    }
    if (name === "execute_console_command" && typeof args.command === "string" && !LEGACY) {
      if (staleTouches && /^p9\.Test\.Touch\s+Start/i.test(args.command)) {
        await releaseActiveTouches("stale pointers from an earlier session");
      }
      trackTouchCommand(args.command);
    }
    // Actor tools that only accept object names get labels from earlier spawns translated.
    if (args.actor_name && labelToName.has(args.actor_name)) {
      args.actor_name = labelToName.get(args.actor_name);
    }
    const res = await request("tools/call", { name, arguments: args });
    const text = res.result?.content?.map((c) => c.text).join("\n") ?? JSON.stringify(res.error);
    if (name === "spawn_actor" && !res.result?.isError) {
      try {
        const spawned = JSON.parse(text);
        if (spawned.label && spawned.name) labelToName.set(spawned.label, spawned.name);
      } catch {}
    }
    const file = join(outDir, `${String(n).padStart(2, "0")}_${name}.txt`);
    writeFileSync(file, text);
    console.log(`${name} ${JSON.stringify(args)} -> ${res.result?.isError ? "ERROR " : ""}${text.length} chars -> ${file}`);
  }
} catch (e) {
  console.error("CALL FAILED:", e.message);
  process.exitCode = 1;
} finally {
  // Every simulated touch start gets a matching end, also after an abort or an exception.
  try {
    await releaseActiveTouches("run cleanup");
  } catch (e) {
    console.log(`[harness] run cleanup failed: ${e.message}`);
  }
  child.kill();
}
