#!/usr/bin/env node
// uemcp-offline.mjs — run any UEMCP OFFLINE tool against this project on disk,
// with no Unreal Editor and no MCP handshake. This is the guaranteed-reliable
// path for cloud/headless sessions.
//
// Usage:
//   node .claude/scripts/uemcp-offline.mjs <tool> [jsonArgs]
//   node .claude/scripts/uemcp-offline.mjs                 # lists offline tools
//
// Examples:
//   node .claude/scripts/uemcp-offline.mjs project_info
//   node .claude/scripts/uemcp-offline.mjs query_asset_registry '{"path_prefix":"/Game/ProjectFiles/Data/PDA/Attack","limit":200}'
//   node .claude/scripts/uemcp-offline.mjs read_asset_properties '{"asset_path":"Content/ProjectFiles/Data/PDA/Attack/AttackConfigurations/DA_Config_Katana.uasset"}'
//
// Env:
//   UEMCP_HOME            server location   (default: $HOME/uemcp)
//   UNREAL_PROJECT_ROOT   project to read   (default: this repo's root)
import { pathToFileURL } from 'node:url';
import { existsSync } from 'node:fs';
import { dirname, resolve, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import os from 'node:os';

const OFFLINE_TOOLS = [
  'project_info', 'get_asset_info', 'query_asset_registry', 'read_asset_properties',
  'list_asset_exports', 'inspect_blueprint', 'list_level_actors', 'list_gameplay_tags',
  'search_gameplay_tags', 'list_config_values', 'bp_list_graphs', 'bp_list_entry_points',
  'bp_find_in_graph', 'bp_show_node', 'bp_trace_exec', 'bp_trace_data',
  'find_blueprint_nodes', 'find_blueprint_nodes_bulk',
];

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, '..', '..'); // .claude/scripts -> repo root
const UEMCP_HOME = process.env.UEMCP_HOME || join(os.homedir(), 'uemcp');
const PROJECT_ROOT = process.env.UNREAL_PROJECT_ROOT || repoRoot;
const offlineToolsPath = join(UEMCP_HOME, 'server', 'offline-tools.mjs');

const [, , tool, rawArgs] = process.argv;

function fail(msg, code = 1) { console.error(`[uemcp-offline] ${msg}`); process.exit(code); }

if (!tool || tool === '-h' || tool === '--help') {
  console.log('UEMCP offline tools:\n  ' + OFFLINE_TOOLS.join('\n  '));
  console.log('\nUsage: node .claude/scripts/uemcp-offline.mjs <tool> [jsonArgs]');
  process.exit(0);
}
if (!existsSync(offlineToolsPath)) {
  fail(`UEMCP not provisioned at ${UEMCP_HOME}. Run: bash .claude/scripts/uemcp-cloud-bootstrap.sh`);
}

let args = {};
if (rawArgs) {
  try { args = JSON.parse(rawArgs); }
  catch (e) { fail(`arguments must be valid JSON: ${e.message}`); }
}

const mod = await import(pathToFileURL(offlineToolsPath).href);
const exec = mod.executeOfflineTool;
if (typeof exec !== 'function') fail('executeOfflineTool export not found in offline-tools.mjs');

try {
  const result = await exec(tool, args, PROJECT_ROOT);
  process.stdout.write(JSON.stringify(result, null, 2) + '\n');
} catch (e) {
  fail(`${tool} failed: ${e.message}`);
}
