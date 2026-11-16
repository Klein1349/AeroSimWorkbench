import { spawnSync } from 'node:child_process';
import { readFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { parameterSchemas as schemas, validateParameterCommand } from './parameter-schema.mjs';

const pages = {
  '控制方程': 'control-equations', '边界条件': 'boundary-conditions',
  '网格运动': 'mesh-motion', '求解方法': 'solution-method',
  '参考量': 'reference-values', '输出控制': 'output-control',
  '初始化': 'initialization', '计算控制': 'calculation-control',
};
class Failure extends Error {
  constructor(code, message, details = {}) {
    super(message);
    this.code = code;
    this.details = details;
  }
}
const fail = (code, message, details) => { throw new Failure(code, message, details); };
const clip = (value, size = 1800) => String(value ?? '').slice(0, size);
let binary;
let bound;
let current;
let preset;
let before = {};
let lastReadback;
let headed = false;
let dryRun = false;
let targetPid;
let cancelFile;
let readyFile;
const attempted = [];
const mutations = [];

function run(args) {
  const result = spawnSync(binary, args, {
    shell: false, encoding: 'utf8', windowsHide: true,
    timeout: 20000, maxBuffer: 12 * 1024 * 1024,
  });
  let envelope;
  try { envelope = JSON.parse(String(result.stdout ?? '').trim()); } catch {
    fail('INVALID_TOOL_OUTPUT', 'agent-desktop did not return a JSON envelope.', {
      command: args[0], exit_status: result.status, process_error: clip(result.error?.message),
      stdout: clip(result.stdout), stderr: clip(result.stderr),
    });
  }
  if (result.error || result.status !== 0 || envelope.ok !== true) {
    fail(envelope.error?.code ?? 'COMMAND_FAILED', envelope.error?.message ?? 'agent-desktop command failed.', {
      command: args[0], exit_status: result.status,
      disposition: envelope.error?.disposition ?? envelope.data?.disposition ?? null,
      tool_details: envelope.error?.details ?? null,
      process_error: clip(result.error?.message), stderr: clip(result.stderr),
    });
  }
  return envelope.data;
}
function nodes(root) {
  const result = [];
  const visit = node => {
    if (!node || typeof node !== 'object') return;
    result.push(node);
    for (const child of node.children ?? []) visit(child);
  };
  visit(root);
  return result;
}
function ancestorPath(root, target) {
  if (root === target) return [root];
  for (const child of root.children ?? []) {
    const path = ancestorPath(child, target);
    if (path) return [root, ...path];
  }
}
function nativeId(node) {
  return typeof node.native_id === 'string' ? node.native_id : String(node.native_id?.value ?? '');
}
function visible(node) {
  return node.visible !== false && !(node.states ?? []).some(state => ['invisible', 'hidden', 'offscreen'].includes(state));
}
function inventory() {
  const data = run(['list-windows', '--app', 'aerosim_gui.exe', '--timeout-ms', '12000']);
  if (!Array.isArray(data)) fail('INVALID_INVENTORY', 'Unexpected list-windows response.');
  return data.filter(window => String(window.app_name).toLowerCase() === 'aerosim_gui.exe'
    && window.visible === true && String(window.title).startsWith('AeroSimWorkbench')
    && (targetPid === undefined || window.pid === targetPid));
}
function assertWindow() {
  const candidates = inventory();
  if (candidates.length !== 1) fail('AMBIGUOUS_WINDOW', 'Keep exactly one visible AeroSimWorkbench main window open.', {
    candidates: candidates.map(({ id, title, pid }) => ({ id, title, pid })),
  });
  const window = candidates[0];
  if (!bound) bound = window;
  if (window.id !== bound.id || window.pid !== bound.pid
      || window.process_instance !== bound.process_instance) {
    fail('WINDOW_CHANGED', 'The bound application instance changed. Start a new configuration run.');
  }
  if (window.minimized || window.accessible === false) fail('WINDOW_UNAVAILABLE', 'Restore the main window and close any blocking dialog before running this command.');
  return window;
}
function snapshot() {
  assertWindow();
  const data = run(['snapshot', '--window-id', bound.id, '--include-bounds', '--compact', '--max-depth', '30', '--timeout-ms', '12000']);
  if (data.complete !== true || !data.tree) fail('INCOMPLETE_SNAPSHOT', 'The complete window tree could not be read; nothing further will be changed.');
  if (data.window?.id !== bound.id) fail('WINDOW_CHANGED', 'Snapshot does not belong to the bound main window.');
  const all = nodes(data.tree);
  if (all.some(node => node.complete === false)) fail('INCOMPLETE_SNAPSHOT', 'A subtree was reported incomplete.');
  current = data;
  return data;
}
function pagePresent(page) {
  const marker = `parameterEditor_${pages[page]}`;
  return nodes(current.tree).some(node => visible(node) && node.name === page
    && nativeId(node).includes(`${marker}.parameterHeading`));
}
function waitFor(observe, message) {
  const deadline = Date.now() + 3000;
  while (!observe()) {
    if (Date.now() >= deadline) fail('POSTCONDITION_FAILED', message);
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 100);
    snapshot();
  }
}
function checkCancelled() {
  if (cancelFile && existsSync(cancelFile)) fail('CANCELLED', 'Configuration stopped at the next safe boundary. Previously applied values were retained.');
}
function unique(name, role) {
  const matches = nodes(current.tree).filter(node => visible(node) && node.name === name && node.role === role);
  if (matches.length !== 1) fail('AMBIGUOUS_FIELD', `Expected one ${role} named ${name}.`, { name, role, count: matches.length });
  const node = matches[0];
  if (node.enabled === false || (node.states ?? []).some(state => ['disabled', 'unavailable'].includes(state)
      || (role === 'incrementor' && ['readonly', 'read_only'].includes(state)))) {
    fail('FIELD_UNAVAILABLE', `The control is disabled or read-only: ${name}.`);
  }
  return node;
}
function ref(node) {
  if (!/^@[^:]+:e\d+$/.test(node.ref_id ?? '')) fail('UNADDRESSABLE_FIELD', `The control has no unique snapshot reference: ${node.name}.`);
  return node.ref_id;
}
function read(node, spec) {
  if (spec.role === 'incrementor') {
    const value = Number(String(node.value ?? '').trim());
    if (!Number.isFinite(value) || String(node.value ?? '').trim() === '') fail('UNREADABLE_VALUE', `Cannot read the number in ${node.name}.`);
    return value;
  }
  if (spec.role === 'checkbox') {
    if (typeof node.value === 'boolean') return node.value;
    if ((node.states ?? []).includes('checked')) return true;
    if ((node.states ?? []).includes('unchecked')) return false;
    fail('UNREADABLE_VALUE', `Cannot read checkbox state: ${node.name}.`);
  }
  if (typeof node.value !== 'string') fail('UNREADABLE_VALUE', `Cannot read the selected option in ${node.name}.`);
  return node.value;
}
function readValues() {
  if (!pagePresent(preset.page)) fail('PAGE_CHANGED', 'The requested parameter page is no longer visible.');
  const values = {};
  for (const [name] of Object.entries(preset.values)) {
    const spec = schemas[preset.page][name];
    values[name] = read(unique(name, spec.role), spec);
  }
  return values;
}
function equal(a, b) {
  return a === b;
}
function action(args) {
  checkCancelled();
  attempted.push(args[0]);
  try {
    const data = run(args);
    mutations.push(args[0]);
    snapshot();
    return data;
  } catch (error) {
    // Never repeat or substitute an action after uncertain delivery, even if values look right.
    try {
      snapshot();
      lastReadback = pagePresent(preset.page) ? readValues() : { page_visible: false };
    } catch (observationError) {
      lastReadback = { error: observationError.code ?? 'READBACK_FAILED', message: clip(observationError.message) };
    }
    error.details = { ...error.details, readback: lastReadback, retried: false };
    throw error;
  }
}
function clickObserved(node) {
  const window = assertWindow();
  if (!window.is_focused) fail('FOCUS_NOT_CONFIRMED', 'The main window must remain in the foreground.');
  const b = node.bounds;
  const w = window.bounds;
  if (!b || !w || !['x', 'y', 'width', 'height'].every(key => Number.isFinite(b[key]))
      || b.width <= 0 || b.height <= 0) fail('MISSING_BOUNDS', 'The control has no usable observed bounds.');
  const x = Math.round(b.x + b.width / 2);
  const y = Math.round(b.y + b.height / 2);
  if (x < w.x || y < w.y || x >= w.x + w.width || y >= w.y + w.height) {
    fail('CONTROL_OUTSIDE_WINDOW', 'The observed control is outside the main window.');
  }
  for (const ancestor of ancestorPath(current.tree, node) ?? []) {
    const clips = nativeId(ancestor).endsWith('.qt_scrollarea_viewport')
      || ['tree', 'listbox'].includes(ancestor.role);
    const viewport = ancestor.bounds;
    // Qt exposes a zero-sized synthetic listbox even for an open combo popup.
    // Its observed option rectangles are usable; real viewport rectangles still clip.
    if (clips && viewport?.width > 0 && viewport?.height > 0 && (x < viewport.x || y < viewport.y
        || x >= viewport.x + viewport.width || y >= viewport.y + viewport.height)) {
      fail('FIELD_CLIPPED', 'The control is outside its visible viewport. Enlarge the window or scroll the parameter into view before running again.',
        { name: node.name, bounds: b, ancestor_role: ancestor.role, ancestor_id: nativeId(ancestor), viewport });
    }
  }
  action(['mouse-click', '--xy', `${x},${y}`, '--headed']);
}
function navigate(page) {
  if (pagePresent(page)) return;
  if (!headed) fail('HEADED_REQUIRED', 'Qt navigation requires --headed; the script will not change focus in headless mode.', { requested_page: page });
  action(['focus-window', '--window-id', bound.id, '--headed']);
  const window = assertWindow();
  if (window.is_focused !== true) fail('FOCUS_NOT_CONFIRMED', 'The main window did not become focused.');
  snapshot();
  clickObserved(unique(page, 'treeitem'));
  waitFor(() => pagePresent(page), 'The click did not open the requested parameter page.');
}
function validatePreset(value) {
  if (value && typeof value.page === 'string' && !Object.hasOwn(pages, value.page)) {
    fail('INVALID_PAGE', 'Only parameter pages are allowed.');
  }
  return validateParameterCommand(value);
}function main() {
  const args = process.argv.slice(2);
  if (args.length === 1 && args[0] === '--help') return {
    ok: true, usage: 'node tools/agent-desktop/configure.mjs <preset.json> [--headed] [--dry-run]',
    supported_fields: schemas, scope: 'window-draft',
    note: 'Dry-run never navigates or changes focus. Open the requested page first. Actual edits require --headed for Qt commit and page round-trip verification.',
  };
  headed = args.includes('--headed');
  dryRun = args.includes('--dry-run');
  const pidArg = args.find(arg => arg.startsWith('--target-pid='));
  const cancelArg = args.find(arg => arg.startsWith('--cancel-file='));
  const readyArg = args.find(arg => arg.startsWith('--ready-file='));
  targetPid = pidArg ? Number(pidArg.slice('--target-pid='.length)) : undefined;
  cancelFile = cancelArg?.slice('--cancel-file='.length);
  readyFile = readyArg?.slice('--ready-file='.length);
  if (pidArg && (!Number.isInteger(targetPid) || targetPid <= 0)) fail('INVALID_ARGS', 'target-pid must be a positive process ID.');
  const unknownFlags = args.filter(arg => arg.startsWith('--') && !['--headed', '--dry-run'].includes(arg)
    && !arg.startsWith('--target-pid=') && !arg.startsWith('--cancel-file=') && !arg.startsWith('--ready-file='));
  const positional = args.filter(arg => !arg.startsWith('--'));
  if (unknownFlags.length || positional.length !== 1) fail('INVALID_ARGS', 'Usage: node tools/agent-desktop/configure.mjs <preset.json> [--headed] [--dry-run]');
  const source = readFileSync(resolve(positional[0]), 'utf8').replace(/^\uFEFF/, '');
  if (Buffer.byteLength(source, 'utf8') > 65536) fail('PRESET_TOO_LARGE', 'Preset must be at most 64 KiB.');
  preset = validatePreset(JSON.parse(source));
  checkCancelled();
  if (readyFile) {
    const readyDeadline = Date.now() + 10000;
    while (!existsSync(readyFile)) {
      checkCancelled();
      if (Date.now() >= readyDeadline) fail('RUNNER_NOT_READY', 'The host did not confirm child-process ownership. No desktop action was started.');
      Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 50);
    }
  }
  if (process.platform !== 'win32' || !['x64', 'arm64'].includes(process.arch)) fail('UNSUPPORTED_PLATFORM', 'This deployment requires Windows x64 or ARM64 and Node.js 18 or newer.');
  if (Number(process.versions.node.split('.')[0]) < 18) fail('UNSUPPORTED_NODE', 'Node.js 18 or newer is required.');
  binary = join(dirname(fileURLToPath(import.meta.url)), 'node_modules', 'agent-desktop', 'bin', `agent-desktop-win32-${process.arch}.exe`);
  if (!existsSync(binary)) fail('MISSING_DEPENDENCY', 'Run npm ci in tools/agent-desktop before using this command.');
  snapshot();
  if (!pagePresent(preset.page)) {
    if (dryRun) fail('DRY_RUN_PAGE_REQUIRED', 'Open the requested parameter page manually before dry-run. No focus or navigation was changed.', { requested_page: preset.page });
    navigate(preset.page);
  }
  // Validate every identity, reference, current value, and observed numeric limit before any edit.
  const plan = [];
  for (const [name, requested] of Object.entries(preset.values)) {
    const spec = schemas[preset.page][name];
    const node = unique(name, spec.role);
    ref(node);
    const oldValue = read(node, spec);
    if (spec.role === 'incrementor') {
      const observedMin = node.min_value ?? node.range?.min;
      const observedMax = node.max_value ?? node.range?.max;
      if ((typeof observedMin === 'number' && requested < observedMin)
          || (typeof observedMax === 'number' && requested > observedMax)) fail('OUT_OF_RANGE', `Requested value is outside the observed range for ${name}.`);
    }
    before[name] = oldValue;
    plan.push({ name, role: spec.role, before: oldValue, requested, change: !equal(oldValue, requested) });
  }
  if (dryRun) return { ok: true, dry_run: true, scope: 'window-draft', page: preset.page,
    window: { id: bound.id, title: bound.title }, before, after: before, verified: false, plan,
    note: 'Read-only plan. No parameter, focus, or navigation changes were made.' };
  if (!headed) fail('HEADED_COMMIT_REQUIRED', 'Qt numeric edits need focus transfer and a page round-trip to verify the window draft. Re-run with --headed; no parameters were changed.', { before, plan });
  action(['focus-window', '--window-id', bound.id, '--headed']);
  for (const change of plan.filter(item => item.change)) {
    const spec = schemas[preset.page][change.name];
    if (spec.role === 'incrementor') {
      clickObserved(unique(change.name, spec.role));
      action(['set-value', ref(unique(change.name, spec.role)), String(change.requested), '--timeout-ms', '12000', '--headed']);
      action(['press', 'tab', '--app', 'aerosim_gui.exe', '--window-id', bound.id, '--headed']);
    } else if (spec.role === 'combobox') {
      // Qt 5.12's semantic Select may report delivery without changing QComboBox.
      // Open the real popup and use its freshly observed option bounds.
      clickObserved(unique(change.name, spec.role));
      waitFor(() => nodes(current.tree).some(node => node.role === 'option'
        && node.name === change.requested && visible(node) && node.bounds?.height > 0), 'The requested option did not become visible.');
      clickObserved(unique(change.requested, 'option'));
      action(['press', 'tab', '--app', 'aerosim_gui.exe', '--window-id', bound.id, '--headed']);
    } else if (spec.role === 'checkbox') {
      action([change.requested ? 'check' : 'uncheck', ref(unique(change.name, spec.role)), '--timeout-ms', '12000', '--headed']);
    }
    waitFor(() => equal(read(unique(change.name, spec.role), spec), change.requested), 'The edited value did not match the request.');
    const observed = read(unique(change.name, spec.role), spec);
    if (!equal(observed, change.requested)) fail('VALUE_MISMATCH', 'The edited value did not match the request.', { name: change.name, requested: change.requested, observed });
  }
  navigate(preset.page === '计算控制' ? '输出控制' : '计算控制');
  navigate(preset.page);
  const after = readValues();
  const verified = Object.entries(preset.values).every(([name, requested]) => equal(after[name], requested));
  if (!verified) fail('DRAFT_VERIFICATION_FAILED', 'Values did not survive the parameter-page round-trip.', { after });
  return { ok: true, scope: 'window-draft', page: preset.page, headed: true,
    window: { id: bound.id, title: bound.title }, before, after, verified: true,
    verification: { focus_transfer: true, page_round_trip: true },
    note: 'Verified the current window draft only. No solver was started and no persistent solver input was written.' };
}

try {
  process.stdout.write(`${JSON.stringify(main(), null, 2)}\n`);
} catch (error) {
  process.stdout.write(`${JSON.stringify({ ok: false, scope: 'window-draft', verified: false,
    page: preset?.page, before, attempted_commands: attempted, completed_mutations: mutations,
    error: { code: error.code ?? 'CONFIGURATION_FAILED', message: clip(error.message), details: error.details ?? {} },
  }, null, 2)}\n`);
  process.exitCode = 1;
}
