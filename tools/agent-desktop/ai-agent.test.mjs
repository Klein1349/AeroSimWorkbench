import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { runDesktopAgent } from './ai-agent.mjs';
import { ScopedDesktopClient, StdioMcpClient } from './desktop-agent-client.mjs';

const key = 'sk-test-agent-secret-never-output';
const request = endpoint => ({ endpoint, model: 'gpt-6-sol', apiKey: key, instruction: '打开网格运动，设置运动体名称为body2', targetPid: 41 });
const schema = properties => ({ type: 'object', additionalProperties: false, properties });
const string = { type: 'string' };
const tools = [
  ['desktop_list_windows', schema({ app: string, timeout_ms: { type: 'integer' } })],
  ['desktop_snapshot', schema({ app: string, window_id: string, root: string, surface: string, include_bounds: { type: 'boolean' }, compact: { type: 'boolean' }, max_depth: { type: 'integer' }, timeout_ms: { type: 'integer' } })],
  ['desktop_find', schema({ app: string, window_id: string, root: string, name: string })],
  ['desktop_click', { ...schema({ ref_id: string }), required: ['ref_id'] }],
  ['desktop_set_value', { ...schema({ ref_id: string, value: string }), required: ['ref_id'] }],
  ['desktop_get', schema({ ref_id: string, property: string })],
  ['desktop_press', schema({ app: string, window_id: string, combo: string, force: { type: 'boolean' } })],
  ['desktop_focus_window', schema({ app: string, window_id: string, title: string })],
  ['desktop_mouse_click', schema({ xy: string, count: { type: 'integer' }, button: string })],
  ['desktop_screenshot', schema({ app: string, window_id: string, output_path: string, screen: { type: 'integer' } })],
  ['desktop_restore', schema({ app: string, window_id: string })],
  ['desktop_launch', schema({ app: string })],
  ['desktop_close_app', schema({ app: string })],
  ['desktop_batch', schema({ commands_json: string })],
].map(([name, inputSchema]) => ({ name, description: `fixture ${name}`, inputSchema }));
const host = { id: 'w-main', pid: 41, app_name: 'aerosim_gui.exe', title: 'AeroSimWorkbench · Test', visible: true, accessible: true,
  minimized: false, is_focused: true, process_instance: 'process-41', bounds: { x: 0, y: 0, width: 1000, height: 800 } };
function fixtureTree(sequence = 1, value = 'body1', privateFocus = false) {
  const ref = number => `@s${sequence}:e${number}`;
  return { role: 'window', name: 'AeroSimWorkbench', native_id: { value: 'AeroSimMainWindow' }, ref_id: ref(1), bounds: host.bounds, children: [
    { role: 'treeitem', name: '网格运动', ref_id: ref(2), bounds: { x: 10, y: 100, width: 120, height: 30 } },
    { role: 'textfield', name: '运动体名称', value, ref_id: ref(3), bounds: { x: 200, y: 150, width: 180, height: 30 } },
    { role: 'button', name: '启动计算', ref_id: ref(4), bounds: { x: 200, y: 200, width: 120, height: 30 } },
    { role: 'group', name: '参数助手', native_id: { value: 'AeroSimMainWindow.parameterAssistant' }, ref_id: ref(5), bounds: { x: 0, y: 700, width: 1000, height: 100 }, children: [
      { role: 'textfield', name: '参数指令', value: `private assistant text ${key}`, states: privateFocus ? ['focused'] : [], ref_id: ref(6), bounds: { x: 100, y: 710, width: 500, height: 30 } }] },
    { role: 'button', name: '关闭', ref_id: ref(7), bounds: { x: 960, y: 0, width: 40, height: 30 } },
  ] };
}
function fakeMcp(options = {}) {
  const calls = []; let sequence = 0, value = 'body1';
  const windows = [{ ...host, minimized: options.minimized ?? false },
    { ...host, id: 'w-other', pid: 999, title: 'AeroSimWorkbench other' }];
  if (options.privateWindow) windows.push({ ...host, id: 'w-settings', title: 'AI 设置' });
  if (options.logWindow) windows.push({ ...host, id: 'w-log', title: 'AI执行日志', is_focused: true, bounds: { x: 800, y: 300, width: 200, height: 200 } });
  return { calls, windows, initialize: async () => tools, close: async () => { calls.push(['close']); },
    call: async (name, args) => {
      calls.push([name, args]);
      let data;
      if (name === 'desktop_list_windows') data = windows;
      else if (name === 'desktop_snapshot') data = { complete: true, window: { id: 'w-main' }, tree: fixtureTree(++sequence, value, options.privateFocus) };
      else if (name === 'desktop_set_value') { value = args.value; data = { delivered: true }; }
      else if (name === 'desktop_get') data = { value };
      else if (name === 'desktop_restore') { windows[0].minimized = false; data = { restored: true }; }
      else if (name === 'desktop_screenshot') data = { width: 1000, height: 800 };
      else data = { delivered: true };
      const result = { structuredContent: { ok: true, data }, isError: false, content: [] };
      if (name === 'desktop_screenshot') result.content.push({ type: 'image', mimeType: 'image/png', data: 'iVBORw0KGgo=' });
      return result;
    } };
}
const completed = output => ({ status: 'completed', error: null, incomplete_details: null, output });
const call = (name, args, index = 1) => ({ type: 'function_call', id: `fc_${index}`, call_id: `call_${index}`, status: 'completed', name, arguments: JSON.stringify(args) });
const message = text => ({ type: 'message', role: 'assistant', status: 'completed', content: [{ type: 'output_text', text }] });
async function withServer(handler, use) {
  const server = createServer(handler); await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  try { return await use(`http://127.0.0.1:${server.address().port}/v1`); }
  finally { server.closeAllConnections(); await new Promise(resolve => server.close(resolve)); }
}
async function readJson(req) { let raw = ''; for await (const chunk of req) raw += chunk; return JSON.parse(raw); }
function send(res, response, sse = false) {
  res.writeHead(200, { 'content-type': sse ? 'text/event-stream' : 'application/json' });
  res.end(sse ? `event: response.completed\ndata: ${JSON.stringify({ type: 'response.completed', response })}\n\n` : JSON.stringify(response));
}

test('MCP definitions are discovered dynamically, while process/global tools are unavailable', async () => {
  const mcp = fakeMcp(), desktop = new ScopedDesktopClient(mcp, 41);
  const definitions = await desktop.initialize();
  assert.ok(definitions.some(tool => tool.name === 'desktop_set_value'));
  assert.equal(definitions.some(tool => tool.name === 'desktop_launch'), false);
  assert.equal(definitions.some(tool => tool.name === 'desktop_batch'), false);
  assert.ok(definitions.every(tool => tool.strict === false));
  assert.deepEqual((await desktop.call('desktop_list_windows')).data.map(window => window.id), ['w-main']);
  const snapshot = await desktop.call('desktop_snapshot');
  assert.equal(JSON.stringify(snapshot).includes('private assistant'), false);
  assert.equal(JSON.stringify(snapshot).includes(key), false);
  await assert.rejects(desktop.call('desktop_get', { ref_id: '@s1:e6' }), error => error.code === 'UNKNOWN_OR_STALE_REF');
  await assert.rejects(desktop.call('desktop_close_app', { app: 'aerosim_gui.exe' }), error => error.code === 'TOOL_SCOPE_DENIED');
});

test('no page or parameter allowlist: arbitrary observed controls may be changed and verified', async () => {
  const mcp = fakeMcp(), desktop = new ScopedDesktopClient(mcp, 41);
  await desktop.initialize(); await desktop.call('desktop_snapshot');
  const changed = await desktop.call('desktop_set_value', { ref_id: '@s1:e3', value: 'body2' });
  assert.ok(JSON.stringify(changed.observation).includes('body2'));
  assert.equal(desktop.actionCount, 1);
  await assert.rejects(desktop.call('desktop_get', { ref_id: '@s1:e3' }), error => error.code === 'UNKNOWN_OR_STALE_REF');
  assert.equal((await desktop.call('desktop_get', { ref_id: '@s2:e3' })).data.value, 'body2');
});

test('Qt click becomes one physical click before a modal Invoke can block', async () => {
  const mcp = fakeMcp(), desktop = new ScopedDesktopClient(mcp, 41);
  await desktop.initialize(); await desktop.call('desktop_snapshot');
  await desktop.call('desktop_click', { ref_id: '@s1:e2' });
  assert.equal(mcp.calls.some(([name]) => name === 'desktop_click'), false);
  assert.deepEqual(mcp.calls.find(([name]) => name === 'desktop_mouse_click')[1], { xy: '70,115' });
});

test('cross-process, private refs, close shortcuts and unsafe coordinates are denied', async () => {
  const mcp = fakeMcp(), desktop = new ScopedDesktopClient(mcp, 41);
  await desktop.initialize(); await desktop.call('desktop_snapshot');
  for (const [name, args] of [
    ['desktop_snapshot', { window_id: 'w-other' }], ['desktop_snapshot', { app: 'cmd.exe' }],
    ['desktop_snapshot', { surface: 'desktop' }], ['desktop_mouse_click', { xy: '1200,100' }],
    ['desktop_mouse_click', { xy: '970,15' }], ['desktop_mouse_click', { xy: '200,720' }],
    ['desktop_press', { combo: 'alt+f4' }], ['desktop_press', { combo: 'win+r' }],
    ['desktop_get', { ref_id: '@unseen:e1' }],
  ]) await assert.rejects(desktop.call(name, args));
  assert.equal(mcp.calls.some(([name]) => ['desktop_mouse_click', 'desktop_press'].includes(name)), false);
});

test('AI settings stop observation; floating logs are hidden rather than blocking the whole host', async () => {
  await assert.rejects(new ScopedDesktopClient(fakeMcp({ privateWindow: true }), 41).initialize(), error => error.code === 'PRIVATE_SURFACE');
  const desktop = new ScopedDesktopClient(fakeMcp({ logWindow: true }), 41);
  await desktop.initialize();
  assert.deepEqual((await desktop.call('desktop_list_windows')).data.map(window => window.id), ['w-main']);
  assert.ok((await desktop.call('desktop_snapshot')).data.tree);
  await assert.rejects(desktop.call('desktop_mouse_click', { xy: '850,350' }), error => error.code === 'PRIVATE_SURFACE');
});

test('restore works for a minimized host, and press checks fresh private focus', async () => {
  const mcp = fakeMcp({ minimized: true }), desktop = new ScopedDesktopClient(mcp, 41);
  await desktop.initialize();
  await desktop.call('desktop_restore', {});
  assert.equal(mcp.windows[0].minimized, false);
  const privateMcp = fakeMcp({ privateFocus: true }), privateDesktop = new ScopedDesktopClient(privateMcp, 41);
  await privateDesktop.initialize(); await privateDesktop.call('desktop_snapshot');
  await assert.rejects(privateDesktop.call('desktop_press', { combo: 'tab' }), error => error.code === 'FOCUS_PRIVATE');
  assert.equal(privateMcp.calls.some(([name]) => name === 'desktop_press'), false);
});

test('HTTP Responses tool loop passes stateless history and complete observations to the next turn', async () => {
  const mcp = fakeMcp(), desktop = new ScopedDesktopClient(mcp, 41), events = []; let turn = 0;
  await withServer(async (req, res) => {
    assert.equal(req.url, '/v1/responses'); assert.equal(req.headers.authorization, `Bearer ${key}`);
    const body = await readJson(req); assert.equal(JSON.stringify(body).includes(key), false);
    assert.equal(body.parallel_tool_calls, false); assert.equal(body.store, false); assert.equal(body.stream, true);
    assert.ok(body.tools.every(tool => tool.strict === false));
    assert.equal(Object.hasOwn(body, 'previous_response_id'), false);
    if (turn++ === 0) {
      assert.ok(JSON.stringify(body.input).includes('运动体名称'));
      send(res, completed([message('正在设置运动体名称。'), call('desktop_set_value', { ref_id: '@s1:e3', value: 'body2' })]), true);
    } else {
      assert.ok(body.input.some(item => item.type === 'function_call_output' && item.output.includes('body2')));
      assert.ok(body.input.some(item => item.type === 'function_call'));
      send(res, completed([message('已将运动体名称改为body2，并回读确认。')]));
    }
  }, async endpoint => {
    const result = await runDesktopAgent(request(endpoint), { desktop, emit: event => events.push(event) });
    assert.equal(result.ok, true, JSON.stringify(result)); assert.equal(result.actions, 1); assert.equal(turn, 2);
    assert.equal(JSON.stringify(events).includes(key), false); assert.equal(JSON.stringify(events).includes('reasoning'), false);
  });
});

test('screenshots are sent as input_image and never as log base64 or text tool output', async () => {
  const desktop = new ScopedDesktopClient(fakeMcp(), 41), events = []; let turn = 0;
  await withServer(async (req, res) => {
    const body = await readJson(req);
    if (turn++ === 0) send(res, completed([call('desktop_screenshot', { window_id: 'w-main' })]));
    else {
      const image = body.input.flatMap(item => item.content ?? []).find(item => item.type === 'input_image');
      assert.equal(image.image_url, 'data:image/png;base64,iVBORw0KGgo=');
      assert.equal(body.input.filter(item => item.type === 'function_call_output').some(item => item.output.includes('iVBOR')), false);
      assert.ok(body.input.some(item => item.type === 'function_call_output' && item.output.includes('observation') && item.output.includes('@s2:e3')));
      send(res, completed([message('已观察当前界面。')]));
    }
  }, async endpoint => {
    assert.equal((await runDesktopAgent(request(endpoint), { desktop, emit: event => events.push(event) })).ok, true);
    assert.equal(JSON.stringify(events).includes('iVBOR'), false);
  });
});

test('screenshot returns the fresh refs that replaced the previous text observation', async () => {
  const desktop = new ScopedDesktopClient(fakeMcp(), 41);
  await desktop.initialize(); await desktop.call('desktop_snapshot');
  const result = await desktop.call('desktop_screenshot');
  assert.equal(result.observation.tree.children.find(node => node.name === '运动体名称').ref_id, '@s2:e3');
  assert.equal((await desktop.call('desktop_get', { ref_id: '@s2:e3', property: 'value' })).data.value, 'body1');
  await assert.rejects(desktop.call('desktop_get', { ref_id: '@s1:e3' }), error => error.code === 'UNKNOWN_OR_STALE_REF');
});

test('safe tool denial returns to the model without desktop mutation', async () => {
  const desktop = new ScopedDesktopClient(fakeMcp(), 41); let turn = 0;
  await withServer(async (req, res) => {
    const body = await readJson(req);
    if (turn++ === 0) send(res, completed([call('desktop_close_app', { app: 'aerosim_gui.exe' })]));
    else { assert.ok(body.input.some(item => item.type === 'function_call_output' && item.output.includes('TOOL_SCOPE_DENIED'))); send(res, completed([message('不能关闭宿主，已停止该动作。')])); }
  }, async endpoint => assert.equal((await runDesktopAgent(request(endpoint), { desktop })).ok, true));
});

test('delivered uncertain returns its disposition and fresh observation without retrying the action', async () => {
  const mcp = fakeMcp();
  const normal = mcp.call; let attempted = 0;
  mcp.call = async (name, args) => name === 'desktop_set_value' ? (++attempted, { isError: true, structuredContent: {
    ok: false, error: { code: 'ACTION_TIMEOUT', disposition: { delivery: 'delivered_uncertain', retry: 'unsafe' } } }, content: [] }) : normal(name, args);
  const desktop = new ScopedDesktopClient(mcp, 41); let turn = 0;
  await withServer(async (req, res) => {
    const body = await readJson(req);
    if (turn++ === 0) send(res, completed([call('desktop_set_value', { ref_id: '@s1:e3', value: 'body2' })]));
    else {
      const output = body.input.find(item => item.type === 'function_call_output');
      assert.ok(output.output.includes('delivered_uncertain')); assert.ok(output.output.includes('observation'));
      send(res, completed([message('动作是否完成不确定，已核对当前界面，请确认。')]));
    }
  }, async endpoint => assert.equal((await runDesktopAgent(request(endpoint), { desktop })).ok, true));
  assert.equal(attempted, 1);
});

test('focus geometry changes invalidate pre-focus coordinates instead of sending physical input', async () => {
  const mcp = fakeMcp(); mcp.windows[0].is_focused = false;
  const normal = mcp.call;
  mcp.call = async (name, args) => {
    if (name === 'desktop_focus_window') { mcp.windows[0].is_focused = true; mcp.windows[0].bounds = { ...host.bounds, x: 100 }; }
    return normal(name, args);
  };
  const desktop = new ScopedDesktopClient(mcp, 41);
  await desktop.initialize(); await desktop.call('desktop_snapshot');
  await assert.rejects(desktop.call('desktop_mouse_click', { xy: '70,115' }), error => error.code === 'WINDOW_GEOMETRY_CHANGED');
  assert.equal(mcp.calls.some(([name]) => name === 'desktop_mouse_click'), false);
});

test('log event sizes are bounded independently from full model observations', async () => {
  const desktop = new ScopedDesktopClient(fakeMcp(), 41), events = [];
  const normal = desktop.call.bind(desktop);
  desktop.call = async (name, args) => {
    const result = await normal(name, args);
    if (name === 'desktop_snapshot') result.data.note = '界面'.repeat(15000);
    return result;
  };
  await withServer((req, res) => send(res, completed([message('说明'.repeat(3000))])), async endpoint => {
    assert.equal((await runDesktopAgent(request(endpoint), { desktop, emit: event => events.push(event) })).ok, true);
    assert.ok(events.every(event => event.message.length <= 4000 && (event.detail?.length ?? 0) <= 16000));
  });
});

test('parallel or truncated tool calls are rejected before any mutation', async () => {
  for (const response of [completed([call('desktop_click', { ref_id: '@s1:e2' }), call('desktop_set_value', { ref_id: '@s1:e3', value: 'body2' }, 2)]),
    { ...completed([call('desktop_set_value', { ref_id: '@s1:e3', value: 'body2' })]), status: 'incomplete' }]) {
    const mcp = fakeMcp(), desktop = new ScopedDesktopClient(mcp, 41);
    await withServer((req, res) => send(res, response), async endpoint => {
      const result = await runDesktopAgent(request(endpoint), { desktop });
      assert.equal(result.ok, false); assert.equal(result.actions, 0);
      assert.equal(mcp.calls.some(([name]) => name === 'desktop_set_value'), false);
    });
  }
});

test('cancel and ready control files prevent desktop access before ownership handshake', async () => {
  const directory = mkdtempSync(join(tmpdir(), 'aerosim-agent-'));
  try {
    const cancelFile = join(directory, 'cancel'), readyFile = join(directory, 'ready'); writeFileSync(cancelFile, 'cancel');
    const mcp = fakeMcp(), desktop = new ScopedDesktopClient(mcp, 41);
    const result = await runDesktopAgent({ ...request('https://example.invalid/v1'), cancelFile, readyFile }, { desktop });
    assert.equal(result.error.code, 'CANCELLED'); assert.equal(mcp.calls.length, 0);
    rmSync(cancelFile);
    const notReady = await runDesktopAgent({ ...request('https://example.invalid/v1'), readyFile }, { desktop, readyTimeoutMs: 25 });
    assert.equal(notReady.error.code, 'HOST_NOT_READY'); assert.equal(mcp.calls.length, 0);
  } finally { rmSync(directory, { recursive: true, force: true }); }
});

test('model timeout and authentication errors are sanitized and close desktop resources', async () => {
  const events = [], mcp = fakeMcp(), desktop = new ScopedDesktopClient(mcp, 41);
  await withServer((req, res) => { res.writeHead(401); res.write(key); }, async endpoint => {
    const result = await runDesktopAgent(request(endpoint), { desktop, emit: event => events.push(event) });
    assert.equal(result.error.code, 'AUTHENTICATION_FAILED'); assert.equal(JSON.stringify({ result, events }).includes(key), false);
    assert.ok(mcp.calls.some(([name]) => name === 'close'));
  });
});

test('model inference timeout and AbortSignal cancellation stop further desktop actions', async () => {
  await withServer((req, res) => { res.writeHead(200, { 'content-type': 'text/event-stream' }); res.write(': waiting\n\n'); }, async endpoint => {
    const timed = await runDesktopAgent(request(endpoint), { desktop: new ScopedDesktopClient(fakeMcp(), 41), modelTimeoutMs: 25 });
    assert.equal(timed.error.code, 'MODEL_TIMEOUT');
    const controller = new AbortController();
    const pending = runDesktopAgent(request(endpoint), { desktop: new ScopedDesktopClient(fakeMcp(), 41), signal: controller.signal });
    setTimeout(() => controller.abort(), 25);
    assert.equal((await pending).error.code, 'CANCELLED');
  });
});

test('a scoped root query retains whole-window privacy protection and new root refs', async () => {
  const desktop = new ScopedDesktopClient(fakeMcp(), 41);
  await desktop.initialize(); await desktop.call('desktop_snapshot');
  const subtree = await desktop.call('desktop_snapshot', { root: '@s1:e3' });
  assert.equal(subtree.data.tree.name, '运动体名称'); assert.equal(subtree.data.tree.ref_id, '@s2:e3');
  await assert.rejects(desktop.call('desktop_mouse_click', { xy: '200,720' }), error => error.code === 'PRIVATE_SURFACE');
});

test('surface snapshots query the requested menu and preserve the whole-window privacy baseline', async () => {
  const mcp = fakeMcp(), normal = mcp.call;
  mcp.call = async (name, args) => {
    if (name === 'desktop_snapshot' && args.surface) {
      mcp.calls.push([name, args]);
      return { structuredContent: { ok: true, data: { complete: true, window: { id: 'w-main' }, surface: args.surface,
        tree: { role: 'menu', name: '运动设置', ref_id: '@menu:e1', bounds: { x: 300, y: 300, width: 200, height: 160 }, children: [
          { role: 'menuitem', name: '设置运动', ref_id: '@menu:e2', bounds: { x: 310, y: 320, width: 160, height: 30 } },
          { role: 'textfield', name: '参数指令', value: 'private assistant content', ref_id: '@menu:e3' },
        ] } } }, content: [] };
    }
    return normal(name, args);
  };
  const desktop = new ScopedDesktopClient(mcp, 41);
  await desktop.initialize();
  const surface = await desktop.call('desktop_snapshot', { surface: 'menu', max_depth: 8 });
  assert.equal(surface.data.surface, 'menu'); assert.equal(surface.data.tree.role, 'menu');
  assert.equal(JSON.stringify(surface).includes('private assistant content'), false);
  assert.equal(mcp.calls.filter(([name]) => name === 'desktop_snapshot').length, 2);
  assert.equal(mcp.calls.find(([name, args]) => name === 'desktop_snapshot' && args.surface)[1].max_depth, 8);
  assert.equal(desktop.snapshots.get('w-main').data.tree.role, 'window');
  assert.equal(desktop.ref('@menu:e2').windowId, 'w-main');
  await assert.rejects(desktop.call('desktop_mouse_click', { xy: '200,720' }), error => error.code === 'PRIVATE_SURFACE');
  await desktop.call('desktop_click', { ref_id: '@menu:e2' });
  assert.ok(mcp.calls.some(([name, args]) => name === 'desktop_mouse_click' && args.xy === '390,335'));
});

test('separate surface window ids must belong to the same target process', async () => {
  for (const owned of [true, false]) {
    const mcp = fakeMcp(), normal = mcp.call;
    if (owned) mcp.windows.push({ ...host, id: 'w-popup', title: '运动选项', is_focused: false });
    mcp.call = async (name, args) => name === 'desktop_snapshot' && args.surface ? { structuredContent: { ok: true, data: {
      complete: true, window: { id: owned ? 'w-popup' : 'w-other' },
      tree: { role: 'listbox', ref_id: '@popup:e1', children: [{ role: 'option', name: '固定轴运动', ref_id: '@popup:e2', bounds: { x: 300, y: 300, width: 100, height: 30 } }] },
    } }, content: [] } : normal(name, args);
    const desktop = new ScopedDesktopClient(mcp, 41); await desktop.initialize();
    if (owned) {
      const result = await desktop.call('desktop_snapshot', { surface: 'popover' });
      assert.equal(result.data.window.id, 'w-popup'); assert.equal(desktop.ref('@popup:e2').windowId, 'w-popup');
      assert.equal(desktop.snapshots.get('w-main').data.tree.role, 'window');
    } else await assert.rejects(desktop.call('desktop_snapshot', { surface: 'popover' }), error => error.code === 'WINDOW_SCOPE_DENIED');
  }
});

test('only explicit not_delivered tool failures are classified as safe no-delivery', async () => {
  for (const delivery of ['not_delivered', 'delivered_uncertain', 'delivered_unverified', 'delivered_verified', undefined]) {
    const mcp = fakeMcp(), normal = mcp.call;
    mcp.call = async (name, args) => name === 'desktop_set_value' ? { isError: true, structuredContent: { ok: false,
      error: { code: 'ACTION_FAILED', disposition: delivery ? { delivery, retry: 'unsafe' } : undefined } }, content: [] } : normal(name, args);
    const desktop = new ScopedDesktopClient(mcp, 41); await desktop.initialize(); await desktop.call('desktop_snapshot');
    await assert.rejects(desktop.call('desktop_set_value', { ref_id: '@s1:e3', value: 'body2' }), error => {
      assert.equal(error.code, delivery === 'not_delivered' ? 'DESKTOP_TOOL_FAILED' : 'DELIVERY_UNCERTAIN');
      assert.equal(error.disposition?.delivery, delivery); return true;
    });
  }
});

test('find cannot re-expose private assistant descendants lacking a private native id', async () => {
  const mcp = fakeMcp(), normal = mcp.call;
  mcp.call = async (name, args) => name === 'desktop_find' ? { structuredContent: { ok: true, data: [
    { role: 'textfield', name: '参数指令', value: 'log injection content', ref_id: '@find:e8' },
  ] }, content: [] } : normal(name, args);
  const desktop = new ScopedDesktopClient(mcp, 41);
  await desktop.initialize(); await desktop.call('desktop_snapshot');
  const result = await desktop.call('desktop_find', { name: '参数指令' });
  assert.deepEqual(result.data, []);
  await assert.rejects(desktop.call('desktop_get', { ref_id: '@find:e8' }), error => error.code === 'UNKNOWN_OR_STALE_REF');
});

test('malformed/truncated SSE never starts the requested tool', async () => {
  for (const stream of ['data: {not-json}\n\n', `data: ${JSON.stringify({ type: 'response.function_call_arguments.delta', delta: '{"ref_id":"@s1:e3"}' })}\n\n`]) {
    const mcp = fakeMcp();
    await withServer((req, res) => { res.writeHead(200, { 'content-type': 'text/event-stream' }); res.end(stream); }, async endpoint => {
      const result = await runDesktopAgent(request(endpoint), { desktop: new ScopedDesktopClient(mcp, 41) });
      assert.equal(result.ok, false); assert.equal(result.actions, 0);
      assert.equal(mcp.calls.some(([name]) => name === 'desktop_set_value'), false);
    });
  }
});

test('provider secret echoes are redacted from logs and subsequent stateless history', async () => {
  const events = []; let turn = 0;
  await withServer(async (req, res) => {
    const body = await readJson(req); assert.equal(JSON.stringify(body).includes(key), false);
    if (turn++ === 0) send(res, completed([message(`准备观察 ${key}`), call('desktop_get', { ref_id: '@s1:e3', property: 'value' })]));
    else send(res, completed([message(`当前值为body1 ${key}`)]));
  }, async endpoint => {
    const result = await runDesktopAgent(request(endpoint), { desktop: new ScopedDesktopClient(fakeMcp(), 41), emit: event => events.push(event) });
    assert.equal(result.ok, true); assert.equal(JSON.stringify({ result, events }).includes(key), false);
  });
});

test('connection diagnostics expose only known transport codes, never raw provider errors', async () => {
  for (const transportCode of ['UND_ERR_SOCKET', `UND_ERR_${key}`, 'UNKNOWN_PROVIDER_CODE']) {
    const events = [];
    const result = await runDesktopAgent(request('https://example.invalid/v1'), { desktop: new ScopedDesktopClient(fakeMcp(), 41),
      emit: event => events.push(event), fetchImpl: async () => { throw new TypeError(`secret ${key}`, { cause: { code: transportCode, message: key } }); } });
    assert.equal(result.error.code, 'MODEL_CONNECTION_FAILED');
    assert.equal(result.error.transportCode, transportCode === 'UND_ERR_SOCKET' ? transportCode : undefined);
    assert.equal(JSON.stringify({ result, events }).includes(key), false);
    assert.equal(JSON.stringify({ result, events }).includes('UNKNOWN_PROVIDER_CODE'), false);
  }
});

test('stdio MCP fake process performs initialize, tools/list and tools/call handshake', async () => {
  const directory = mkdtempSync(join(tmpdir(), 'aerosim-mcp-'));
  try {
    const path = join(directory, 'fixture.mjs');
    writeFileSync(path, `import {createInterface} from 'node:readline';\nconst tools=${JSON.stringify(tools)};\nconst host=${JSON.stringify(host)};\nconst tree=${JSON.stringify(fixtureTree())};\ncreateInterface({input:process.stdin}).on('line',line=>{const x=JSON.parse(line);if(!x.id)return;let result;if(x.method==='initialize')result={protocolVersion:'2024-11-05',serverInfo:{name:'fixture',version:'1'},capabilities:{tools:{}}};else if(x.method==='tools/list')result={tools};else if(x.method==='tools/call')result={structuredContent:{ok:true,data:x.params.name==='desktop_list_windows'?[host]:{complete:true,window:{id:'w-main'},tree}},isError:false,content:[]};process.stdout.write(JSON.stringify({jsonrpc:'2.0',id:x.id,result})+'\\n');});`);
    const mcp = new StdioMcpClient({ command: process.execPath, args: [path], timeoutMs: 2000 });
    try {
      const desktop = new ScopedDesktopClient(mcp, 41);
      assert.ok((await desktop.initialize()).some(tool => tool.name === 'desktop_snapshot'));
      assert.ok((await desktop.call('desktop_snapshot')).data.tree);
    } finally { await mcp.close(); }
  } finally { rmSync(directory, { recursive: true, force: true }); }
});
