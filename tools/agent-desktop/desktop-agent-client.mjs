import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

export class DesktopAgentError extends Error {
  constructor(code, message) { super(message); this.code = code; }
}
const fail = (code, message) => { throw new DesktopAgentError(code, message); };
const object = value => value !== null && typeof value === 'object' && !Array.isArray(value);
const nativeId = node => typeof node?.native_id === 'string' ? node.native_id : String(node?.native_id?.value ?? '');
const privateText = value => /parameterAssistant|parameterAgentLog|parameterAgentOutput|parameterAiSettings|parameterAiApiKey|parameterAiShowKey/i.test(value);
const settingsTitle = value => /AI\s*设置|AI\s*Settings/i.test(String(value ?? ''));
const logTitle = value => /参数助手.*(?:日志|结果)|AI.*(?:操作日志|运行日志|执行日志)/i.test(String(value ?? ''));
const privateTitle = value => settingsTitle(value) || logTitle(value);
const visible = node => node?.visible !== false && !(node?.states ?? []).some(state => ['hidden', 'invisible', 'offscreen'].includes(state));
const nodes = value => {
  const found = [];
  const visit = node => { if (!object(node)) return; found.push(node); for (const child of node.children ?? []) visit(child); };
  visit(value);
  return found;
};
const hasPoint = (bounds, x, y) => bounds && ['x', 'y', 'width', 'height'].every(key => Number.isFinite(bounds[key]))
  && bounds.width > 0 && bounds.height > 0 && x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height;
const allowedNames = new Set(['desktop_snapshot', 'desktop_find', 'desktop_screenshot', 'desktop_get', 'desktop_is',
  'desktop_click', 'desktop_double_click', 'desktop_triple_click', 'desktop_right_click', 'desktop_type', 'desktop_set_value',
  'desktop_clear', 'desktop_focus', 'desktop_select', 'desktop_toggle', 'desktop_check', 'desktop_uncheck', 'desktop_expand',
  'desktop_collapse', 'desktop_scroll', 'desktop_scroll_to', 'desktop_press', 'desktop_hover', 'desktop_drag',
  'desktop_mouse_move', 'desktop_mouse_click', 'desktop_mouse_wheel', 'desktop_list_windows', 'desktop_focus_window',
  'desktop_resize_window', 'desktop_move_window', 'desktop_minimize', 'desktop_maximize', 'desktop_restore', 'desktop_wait']);
const observations = new Set(['desktop_snapshot', 'desktop_find', 'desktop_screenshot', 'desktop_get', 'desktop_is', 'desktop_list_windows', 'desktop_wait']);
const refKeys = ['ref_id', 'root', 'from', 'to', 'element'];

export class StdioMcpClient {
  constructor(options = {}) {
    const binary = options.command ?? fileURLToPath(new URL(`./node_modules/agent-desktop/bin/agent-desktop-win32-${process.arch}.exe`, import.meta.url));
    const env = Object.fromEntries(Object.entries(options.env ?? process.env).filter(([key]) => !/(?:API_KEY|ACCESS_TOKEN|SECRET)$/i.test(key)));
    this.child = spawn(binary, options.args ?? ['mcp', '--headed'], { windowsHide: true, shell: false, stdio: ['pipe', 'pipe', 'pipe'], env });
    this.pending = new Map(); this.nextId = 0; this.buffer = ''; this.closed = false;
    this.timeoutMs = options.timeoutMs ?? 25000;
    this.child.stdout.setEncoding('utf8');
    this.child.stdout.on('data', chunk => {
      this.buffer += chunk;
      if (Buffer.byteLength(this.buffer, 'utf8') > 12 * 1024 * 1024) { this.abort('MCP_RESPONSE_TOO_LARGE', '桌面工具响应过长，已停止。'); return; }
      let newline;
      while ((newline = this.buffer.indexOf('\n')) >= 0) {
        const line = this.buffer.slice(0, newline); this.buffer = this.buffer.slice(newline + 1);
        let message;
        try { message = JSON.parse(line); } catch { this.abort('INVALID_MCP_RESPONSE', '桌面工具返回了无效协议数据。'); return; }
        const pending = this.pending.get(message.id);
        if (!pending) {
          if (message.method && message.id !== undefined) this.child.stdin.write(`${JSON.stringify({ jsonrpc: '2.0', id: message.id, error: { code: -32601, message: 'Unsupported client method' } })}\n`);
          continue;
        }
        this.pending.delete(message.id); clearTimeout(pending.timer);
        if (message.error) pending.reject(new DesktopAgentError('MCP_REQUEST_FAILED', '桌面工具请求失败。'));
        else pending.resolve(message.result);
      }
    });
    this.child.stderr.on('data', () => { /* Never forward debug output or inherited secrets. */ });
    this.child.on('error', () => this.abort('MCP_START_FAILED', '无法启动桌面工具。'));
    this.child.on('exit', () => this.abort('MCP_EXITED', '桌面工具进程已退出。'));
    this.child.stdin.on('error', () => this.abort('MCP_PIPE_FAILED', '桌面工具连接已断开。'));
  }
  abort(code, message) {
    if (this.closed) return;
    this.closed = true;
    for (const pending of this.pending.values()) { clearTimeout(pending.timer); pending.reject(new DesktopAgentError(code, message)); }
    this.pending.clear();
    this.child.kill();
  }
  request(method, params = {}, signal) {
    if (this.closed || signal?.aborted) return Promise.reject(new DesktopAgentError('CANCELLED', '桌面连接已停止。'));
    const id = ++this.nextId;
    return new Promise((resolve, reject) => {
      const abort = () => this.abort('CANCELLED', '已停止桌面操作。');
      const timer = setTimeout(() => this.abort('MCP_TIMEOUT', '桌面工具响应超时，已停止后续操作。'), this.timeoutMs);
      signal?.addEventListener('abort', abort, { once: true });
      const finish = callback => value => { signal?.removeEventListener('abort', abort); callback(value); };
      this.pending.set(id, { resolve: finish(resolve), reject: finish(reject), timer });
      this.child.stdin.write(`${JSON.stringify({ jsonrpc: '2.0', id, method, params })}\n`);
    });
  }
  async initialize(signal) {
    await this.request('initialize', { protocolVersion: '2024-11-05', capabilities: {}, clientInfo: { name: 'aerosim-ui-agent', version: '1' } }, signal);
    this.child.stdin.write(`${JSON.stringify({ jsonrpc: '2.0', method: 'notifications/initialized', params: {} })}\n`);
    const tools = []; let cursor;
    do {
      const result = await this.request('tools/list', cursor ? { cursor } : {}, signal);
      if (!Array.isArray(result?.tools)) fail('INVALID_MCP_RESPONSE', '桌面工具没有返回工具定义。');
      tools.push(...result.tools); cursor = result.nextCursor;
      if (tools.length > 256) fail('INVALID_MCP_RESPONSE', '桌面工具清单过长。');
    } while (cursor);
    return tools;
  }
  call(name, args, signal) { return this.request('tools/call', { name, arguments: args }, signal); }
  async close() {
    this.abort('CANCELLED', '桌面连接已关闭。');
    if (this.child.exitCode !== null || this.child.signalCode !== null) return;
    await new Promise(resolve => { const timer = setTimeout(resolve, 1500); this.child.once('exit', () => { clearTimeout(timer); resolve(); }); });
  }
}

function validateArguments(value, schema) {
  if (schema?.type === 'object') {
    if (!object(value)) fail('INVALID_TOOL_ARGUMENTS', '工具参数应为JSON对象。');
    for (const key of schema.required ?? []) if (!Object.hasOwn(value, key)) fail('INVALID_TOOL_ARGUMENTS', '工具缺少必需参数。');
    for (const [key, child] of Object.entries(value)) {
      if (!Object.hasOwn(schema.properties ?? {}, key)) fail('INVALID_TOOL_ARGUMENTS', '工具包含未声明参数。');
      validateArguments(child, schema.properties[key]);
    }
  } else if (schema?.type === 'array') {
    if (!Array.isArray(value) || value.length > 100) fail('INVALID_TOOL_ARGUMENTS', '工具数组参数无效。');
    for (const child of value) validateArguments(child, schema.items);
  } else if (schema?.type === 'string' && (typeof value !== 'string' || value.length > 16384)) fail('INVALID_TOOL_ARGUMENTS', '工具文本参数无效或过长。');
  else if (schema?.type === 'boolean' && typeof value !== 'boolean') fail('INVALID_TOOL_ARGUMENTS', '工具布尔参数无效。');
  else if (['number', 'integer'].includes(schema?.type) && (typeof value !== 'number' || !Number.isFinite(value) || (schema.type === 'integer' && !Number.isInteger(value)))) fail('INVALID_TOOL_ARGUMENTS', '工具数字参数无效。');
  if (schema?.enum && !schema.enum.includes(value)) fail('INVALID_TOOL_ARGUMENTS', '工具选项无效。');
}

function envelope(result) {
  let value = result?.structuredContent;
  if (!value) {
    const text = result?.content?.filter(item => item.type === 'text').map(item => item.text).join('\n');
    try { value = JSON.parse(text); } catch { fail('INVALID_MCP_RESPONSE', '桌面工具未返回结构化结果。'); }
  }
  if (!object(value) || typeof value.ok !== 'boolean') fail('INVALID_MCP_RESPONSE', '桌面工具结果缺少执行状态。');
  return value;
}

export class ScopedDesktopClient {
  constructor(mcp, targetPid, options = {}) {
    this.mcp = mcp; this.targetPid = targetPid; this.signal = options.signal;
    this.emit = options.emit ?? (() => {}); this.tools = new Map(); this.refs = new Map(); this.snapshots = new Map(); this.surfaces = new Map();
    this.windows = []; this.host = null; this.actionCount = 0; this.lastObservationAt = 0;
  }
  async raw(name, args) {
    const result = await this.mcp.call(name, args, this.signal);
    const value = envelope(result);
    if (result.isError || !value.ok) {
      const disposition = value.error?.disposition ?? value.error?.details?.disposition ?? value.data?.disposition;
      const delivery = typeof disposition === 'string' ? disposition : disposition?.delivery;
      // Only an explicit no-delivery disposition proves a failed action did not land.
      const uncertain = delivery !== 'not_delivered';
      const error = new DesktopAgentError(uncertain ? 'DELIVERY_UNCERTAIN' : 'DESKTOP_TOOL_FAILED',
        uncertain ? '桌面动作是否完成不确定，请先观察，不能重复动作。' : '桌面工具未完成该操作。');
      error.toolCode = value.error?.code; error.disposition = disposition; throw error;
    }
    return { data: value.data, images: (result.content ?? []).filter(item => item.type === 'image') };
  }
  async initialize() {
    for (const tool of await this.mcp.initialize(this.signal)) {
      if (allowedNames.has(tool.name) && tool.inputSchema?.type === 'object') this.tools.set(tool.name, tool);
    }
    for (const name of ['desktop_list_windows', 'desktop_snapshot']) if (!this.tools.has(name)) fail('MISSING_DESKTOP_TOOL', '桌面工具缺少窗口观察能力。');
    await this.inventory();
    return [...this.tools.values()].map(tool => ({ type: 'function', name: tool.name, description: tool.description,
      parameters: tool.inputSchema, strict: false }));
  }
  async inventory() {
    const { data } = await this.raw('desktop_list_windows', { app: 'aerosim_gui.exe', timeout_ms: 12000 });
    if (!Array.isArray(data)) fail('INVALID_WINDOW_INVENTORY', '无法核对工作台窗口。');
    const owned = data.filter(window => window.pid === this.targetPid && String(window.app_name).toLowerCase() === 'aerosim_gui.exe' && window.visible !== false);
    if (owned.some(window => settingsTitle(window.title))) fail('PRIVATE_SURFACE', '请先关闭AI设置窗口，再使用桌面助手。');
    this.privateWindows = owned.filter(window => logTitle(window.title));
    this.windows = owned.filter(window => !logTitle(window.title));
    const mains = this.windows.filter(window => String(window.title).startsWith('AeroSimWorkbench'));
    if (mains.length !== 1) fail('HOST_UNAVAILABLE', '当前工作台主窗口不存在或无法唯一确认。');
    const main = mains[0];
    if (this.host && (main.id !== this.host.id || main.process_instance !== this.host.process_instance)) fail('HOST_CHANGED', '工作台进程或主窗口发生变化，已停止。');
    this.host = main;
    return this.windows.map(window => ({ ...window }));
  }
  window(id, allowMinimized = false) {
    const window = this.windows.find(window => window.id === id);
    if (!window || (!allowMinimized && window.minimized) || window.accessible === false) fail('WINDOW_SCOPE_DENIED', '目标窗口不属于当前可操作的工作台。');
    return window;
  }
  blocked(node, window) {
    const id = nativeId(node);
    if (privateText(id) || privateTitle(node.name) || (node.states ?? []).some(state => ['protected', 'password'].includes(state))
        || /password/i.test(node.role ?? '')) return true;
    const close = /^(?:退出(?:程序|应用|工作台)?|exit|quit)$/i.test(node.name ?? '')
      || (/^(?:关闭(?:窗口|程序|工作台)?|close)$/i.test(node.name ?? '') && node.bounds?.y < window.bounds?.y + 90);
    return window.id === this.host.id && close;
  }
  cleanTree(node, window, blocked = [], privateNodes = []) {
    if (!object(node)) return node;
    const privateMatch = privateNodes.some(privateNode => node.ref_id && privateNode.ref_id === node.ref_id
      || (node.name && privateNode.name === node.name && privateNode.role === node.role
        && (!node.bounds || JSON.stringify(node.bounds) === JSON.stringify(privateNode.bounds))));
    if (this.blocked(node, window) || privateMatch) { blocked.push(node); return null; }
    return { ...node, children: (node.children ?? []).map(child => this.cleanTree(child, window, blocked, privateNodes)).filter(Boolean) };
  }
  register(value, window, clear = false) {
    if (clear) for (const [ref, entry] of this.refs) if (entry.windowId === window.id) this.refs.delete(ref);
    const visit = node => {
      if (!object(node)) return;
      if (typeof node.ref_id === 'string' && /^@[^:]+:e\d+$/.test(node.ref_id)) this.refs.set(node.ref_id, { node, windowId: window.id });
      for (const child of Object.values(node)) {
        if (Array.isArray(child)) child.forEach(visit); else if (object(child)) visit(child);
      }
    };
    visit(value);
  }
  async snapshot(windowId = this.host.id, args = {}) {
    const window = this.window(windowId);
    const requestedRoot = args.root ? this.ref(args.root).node : null;
    // Baselines always cover the whole window so a subtree query cannot hide private siblings.
    const { data } = await this.raw('desktop_snapshot', { app: 'aerosim_gui.exe', window_id: windowId, include_bounds: true, compact: true, max_depth: 30, timeout_ms: 12000 });
    if (data?.complete !== true || !data.tree || data.window?.id !== windowId || nodes(data.tree).some(node => node.complete === false)) fail('INCOMPLETE_SNAPSHOT', '工作台观察不完整，已停止进一步操作。');
    if (nodes(data.tree).some(node => /parameterAiSettingsDialog/i.test(nativeId(node)))) fail('PRIVATE_SURFACE', 'AI设置窗口不能交给桌面助手观察。');
    const blocked = [];
    const tree = this.cleanTree(data.tree, window, blocked);
    const safe = { ...data, tree };
    this.snapshots.set(windowId, { data: safe, blocked, privateFocus: blocked.some(node => nodes(node).some(child => (child.states ?? []).includes('focused'))) });
    for (const [key, surface] of this.surfaces) if (surface.ownerWindowId === windowId) this.surfaces.delete(key);
    this.register(tree, window, true); this.lastObservationAt = Date.now();
    let observed = safe;
    if (args.surface && args.surface !== 'window') {
      const { data: surfaceData } = await this.raw('desktop_snapshot', { app: 'aerosim_gui.exe', window_id: windowId,
        surface: args.surface, include_bounds: true, compact: args.compact ?? true, max_depth: Math.min(30, Math.max(1, args.max_depth ?? 30)), timeout_ms: 12000 });
      if (surfaceData?.complete !== true || !surfaceData.tree || typeof surfaceData.window?.id !== 'string'
          || nodes(surfaceData.tree).some(node => node.complete === false)) fail('INCOMPLETE_SNAPSHOT', '工作台弹出界面观察不完整，请重新观察。');
      // Some adapters report a separate menu/popover window. Its ownership must also be verified.
      if (surfaceData.window.id !== windowId) await this.inventory();
      const surfaceWindow = this.window(surfaceData.window.id);
      if (nodes(surfaceData.tree).some(node => /parameterAiSettingsDialog/i.test(nativeId(node)))) fail('PRIVATE_SURFACE', 'AI设置窗口不能交给桌面助手观察。');
      const surfaceBlocked = [];
      const privateNodes = blocked.flatMap(node => nodes(node));
      observed = { ...surfaceData, tree: this.cleanTree(surfaceData.tree, surfaceWindow, surfaceBlocked, privateNodes) };
      this.surfaces.set(`${windowId}:${args.surface}`, { ownerWindowId: windowId, windowId: surfaceWindow.id,
        data: observed, blocked: surfaceBlocked });
      if (surfaceWindow.id !== windowId) this.snapshots.set(surfaceWindow.id, { data: observed, blocked: surfaceBlocked,
        privateFocus: surfaceBlocked.some(node => nodes(node).some(child => (child.states ?? []).includes('focused'))) });
      this.register(observed.tree, surfaceWindow);
      // Keep the whole-window privacy baseline; menu queries must not erase private siblings.
    }
    if (requestedRoot) {
      const matches = nodes(observed.tree).filter(node => node.role === requestedRoot.role && node.name === requestedRoot.name
        && (nativeId(requestedRoot) ? nativeId(node) === nativeId(requestedRoot) : JSON.stringify(node.bounds) === JSON.stringify(requestedRoot.bounds)));
      if (matches.length !== 1) fail('UNKNOWN_OR_STALE_REF', '查询根节点发生变化，请重新观察窗口。');
      return { ...observed, tree: matches[0] };
    }
    return observed;
  }
  ref(value) {
    const entry = this.refs.get(value);
    if (!entry) fail('UNKNOWN_OR_STALE_REF', '目标ref不在当前工作台最新观察中，请重新观察。');
    this.window(entry.windowId);
    if (this.blocked(entry.node, this.window(entry.windowId))) fail('PRIVATE_SURFACE', '不能操作助手、密钥或关闭宿主的控件。');
    return entry;
  }
  point(value, preferred) {
    const pair = typeof value === 'string' ? /^(-?\d+(?:\.\d+)?),\s*(-?\d+(?:\.\d+)?)$/.exec(value.trim()) : null;
    if (!pair) fail('INVALID_COORDINATES', '屏幕坐标格式无效。');
    const x = Number(pair[1]), y = Number(pair[2]);
    const candidates = this.windows.filter(window => (!preferred || window.id === preferred) && hasPoint(window.bounds, x, y));
    const window = candidates.find(window => window.is_focused) ?? (candidates.length === 1 ? candidates[0] : null);
    if (!window) fail('WINDOW_SCOPE_DENIED', '坐标不能唯一定位到当前工作台窗口。');
    const snapshot = this.snapshots.get(window.id);
    const surfaces = [...this.surfaces.values()].filter(surface => surface.windowId === window.id);
    if ((this.privateWindows ?? []).some(privateWindow => hasPoint(privateWindow.bounds, x, y))) fail('PRIVATE_SURFACE', '坐标被助手日志窗口遮挡，请先移开日志窗口。');
    if ((!snapshot && !surfaces.length) || (snapshot?.blocked ?? []).concat(surfaces.flatMap(surface => surface.blocked)).some(node => hasPoint(node.bounds, x, y))) fail('PRIVATE_SURFACE', '该位置不可操作或尚未观察。');
    if (window.id === this.host.id && x > window.bounds.x + window.bounds.width - 80 && y < window.bounds.y + 80) fail('HOST_CLOSE_DENIED', '不能通过窗口标题栏关闭宿主。');
    const observedTrees = [snapshot?.data.tree, ...surfaces.map(surface => surface.data.tree)].filter(Boolean);
    if (!observedTrees.some(tree => nodes(tree).some(node => visible(node) && hasPoint(node.bounds, x, y) && node !== tree))) fail('UNOBSERVED_COORDINATES', '坐标未落在已观察的工作台控件内。');
    return window.id;
  }
  async focus(windowId) {
    if (!this.window(windowId).is_focused) {
      await this.raw('desktop_focus_window', { app: 'aerosim_gui.exe', window_id: windowId });
      await this.inventory();
    }
    if (!this.window(windowId).is_focused) fail('FOCUS_NOT_CONFIRMED', '无法确认工作台位于前台，已停止物理输入。');
  }
  async call(name, originalArgs = {}) {
    const tool = this.tools.get(name);
    if (!tool) fail('TOOL_SCOPE_DENIED', '该工具不能用于当前工作台任务。');
    validateArguments(originalArgs, tool.inputSchema);
    const args = { ...originalArgs };
    if (args.app && String(args.app).toLowerCase() !== 'aerosim_gui.exe') fail('WINDOW_SCOPE_DENIED', '不能操作其他应用。');
    if (args.surface && !['window', 'menu', 'menubar', 'sheet', 'popover', 'alert', 'toolbar'].includes(args.surface)) fail('WINDOW_SCOPE_DENIED', '不能观察或操作系统桌面。');
    if (args.output_path || args.screen !== undefined) fail('WINDOW_SCOPE_DENIED', '截图仅限当前工作台，不允许全屏或写入任意路径。');
    if (args.force) fail('HOST_CLOSE_DENIED', '不能强制发送危险快捷键。');
    for (const key of ['timeout_ms', 'timeout']) if (args[key] !== undefined) args[key] = Math.min(12000, Math.max(0, args[key]));
    await this.inventory();
    if (name === 'desktop_list_windows') return { data: this.windows };
    let windowId = args.window_id ?? this.windows.find(window => window.is_focused)?.id ?? this.host.id;
    this.window(windowId, name === 'desktop_restore');
    for (const key of refKeys) if (args[key] !== undefined) windowId = this.ref(args[key]).windowId;
    if (name === 'desktop_snapshot') return { data: await this.snapshot(windowId, args) };
    if (name === 'desktop_find') {
      // Snapshot ownership is established before exposing any refs returned by a search.
      if (!this.snapshots.has(windowId)) await this.snapshot(windowId);
      const result = await this.raw(name, { ...args, app: 'aerosim_gui.exe', window_id: windowId });
      const baseline = this.snapshots.get(windowId);
      const privateNodes = baseline.blocked.flatMap(node => nodes(node));
      const clean = value => {
        if (Array.isArray(value)) return value.map(clean).filter(value => value !== undefined);
        if (!object(value)) return value;
        if (this.blocked(value, this.window(windowId))) return undefined;
        if (value.ref_id && privateNodes.some(node => node.ref_id === value.ref_id
            || (value.name && node.name === value.name && node.role === value.role
              && (!value.bounds || JSON.stringify(value.bounds) === JSON.stringify(node.bounds))))) return undefined;
        return Object.fromEntries(Object.entries(value).map(([key, child]) => [key, clean(child)]));
      };
      const data = clean(result.data);
      this.register(data, this.window(windowId));
      return { data };
    }
    if (name === 'desktop_restore' && this.window(windowId, true).minimized) {
      const result = await this.raw(name, { ...args, app: 'aerosim_gui.exe', window_id: windowId });
      ++this.actionCount; await this.inventory();
      return { data: result.data, observation: await this.snapshot(windowId) };
    }
    if (!this.snapshots.has(windowId)) await this.snapshot(windowId);
    if (name === 'desktop_press') {
      if (/(?:^|[+\s])(?:meta|super|win|cmd)(?:[+\s]|$)|alt\+(?:f4|tab|space)|ctrl\+(?:q|w|esc)|ctrl\+alt\+delete/i.test(args.combo)) fail('HOST_CLOSE_DENIED', '不能发送关闭宿主或切换系统的快捷键。');
    }
    if (name === 'desktop_wait' && (args.notification || args.event || args.menu || args.menu_closed || args.window || args.text)) fail('WINDOW_SCOPE_DENIED', '等待仅支持当前工作台ref或短暂延时。');
    if (name === 'desktop_wait' && args.ms !== undefined) args.ms = Math.min(3000, Math.max(0, args.ms));
    if (name === 'desktop_drag') {
      const from = args.from ? this.ref(args.from).windowId : this.point(args.from_xy, windowId);
      const to = args.to ? this.ref(args.to).windowId : this.point(args.to_xy, windowId);
      if (from !== to) fail('WINDOW_SCOPE_DENIED', '拖放仅限同一工作台窗口。');
      windowId = from;
    } else if (args.xy !== undefined) windowId = this.point(args.xy, windowId);
    else if (name === 'desktop_mouse_wheel') windowId = this.point(`${args.x},${args.y}`, windowId);
    const scopedArgs = { ...args };
    if (Object.hasOwn(tool.inputSchema.properties ?? {}, 'window_id')) scopedArgs.window_id = windowId;
    if (Object.hasOwn(tool.inputSchema.properties ?? {}, 'app')) scopedArgs.app = 'aerosim_gui.exe';
    const mutation = !observations.has(name);
    if (mutation) {
      const previousBounds = JSON.stringify(this.window(windowId).bounds);
      await this.focus(windowId);
      if (JSON.stringify(this.window(windowId).bounds) !== previousBounds) {
        await this.snapshot(windowId);
        fail('WINDOW_GEOMETRY_CHANGED', '窗口聚焦后位置发生变化，请根据最新观察重新选择坐标。');
      }
      if (name === 'desktop_press') {
        await this.snapshot(windowId);
        if (this.snapshots.get(windowId)?.privateFocus) fail('FOCUS_PRIVATE', '当前键盘焦点位于助手私有区域，请先用desktop_focus选择工作台参数控件，然后重读界面。');
      }
    }
    let actualName = name;
    let actualArgs = scopedArgs;
    const clicked = args.ref_id ? this.ref(args.ref_id).node : null;
    if (name === 'desktop_click' && clicked && ['button', 'treeitem', 'combobox', 'menuitem', 'tab', 'option'].includes(clicked.role)) {
      const bounds = clicked.bounds;
      if (!bounds || bounds.width <= 0 || bounds.height <= 0) fail('MISSING_BOUNDS', '控件缺少可用于Qt物理点击的位置。');
      const xy = `${Math.round(bounds.x + bounds.width / 2)},${Math.round(bounds.y + bounds.height / 2)}`;
      this.point(xy, windowId);
      if (!this.tools.has('desktop_mouse_click')) fail('MISSING_DESKTOP_TOOL', '缺少Qt所需的物理点击工具。');
      actualName = 'desktop_mouse_click'; actualArgs = { xy };
    }
    let screenshotObservation;
    if (name === 'desktop_screenshot') {
      // Settings are denied by inventory. Application screenshots may include its visible,
      // already-sanitized assistant/log UI; image data is never logged as text.
      screenshotObservation = await this.snapshot(windowId);
    }
    const result = await this.raw(actualName, actualArgs);
    if (screenshotObservation) return { ...result, observation: screenshotObservation, windowId };
    if (mutation) {
      ++this.actionCount;
      this.refs.clear(); this.snapshots.clear(); this.surfaces.clear();
      await this.inventory();
      const active = this.windows.find(window => window.is_focused) ?? this.windows.find(window => window.id === windowId) ?? this.host;
      const observation = await this.snapshot(active.id);
      return { data: result.data, observation, windowId: active.id };
    }
    return result;
  }
  close() { return this.mcp.close(); }
}
