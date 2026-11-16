import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { planParameters, parseModelPlan, responsesEndpoint } from './ai-plan.mjs';
import { parameterSchemas, validateParameterCommand } from './parameter-schema.mjs';

const apiKey = 'sk-private-test-secret-123456789';
const plan = { summary: '按要求设置迭代步数和马赫数', commands: [
  { page: '计算控制', values: { '最大迭代步数': 12000, 'GPU 计算': '不使用GPU计算' } },
  { page: '参考量', values: { '马赫数': 0.8, '攻角 (°)': -3.5 } },
] };
const request = endpoint => ({ endpoint, model: 'gpt-6-sol', apiKey, instruction: '最大迭代步数12000，GPU关闭；马赫数0.8，攻角-3.5度' });
const completed = text => ({ id: 'resp_local_test', object: 'response', status: 'completed', error: null,
  incomplete_details: null, output: [{ type: 'message', role: 'assistant', status: 'completed', content: [{ type: 'output_text', text }] }] });
const output = (response, value) => { response.writeHead(200, { 'content-type': 'application/json' }); response.end(JSON.stringify(value)); };

async function withServer(handler, use) {
  const server = createServer(handler);
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  try { return await use(`http://127.0.0.1:${server.address().port}/v1`); }
  finally {
    server.closeAllConnections();
    await new Promise(resolve => server.close(resolve));
  }
}

test('Responses endpoint uses HTTPS except exact loopback and never accepts credentials or query keys', () => {
  assert.equal(responsesEndpoint('https://example.invalid/v1/'), 'https://example.invalid/v1/responses');
  assert.equal(responsesEndpoint('https://example.invalid/v1/responses'), 'https://example.invalid/v1/responses');
  assert.equal(responsesEndpoint('http://localhost:1234/v1'), 'http://localhost:1234/v1/responses');
  assert.equal(responsesEndpoint('http://[::1]:1234/v1/'), 'http://[::1]:1234/v1/responses');
  for (const url of ['http://example.invalid/v1', 'https://key@example.invalid/v1', 'https://example.invalid/v1?api_key=secret',
    'https://example.invalid/v1#secret', 'file:///c:/secret', 'http://localhost.attacker.invalid/v1', 'https://example.invalid/v1/chat/completions']) {
    assert.throws(() => responsesEndpoint(url));
  }
});

test('sends native Responses with bearer header, user protocol and no key in request body', async () => {
  await withServer(async (req, res) => {
    assert.equal(req.method, 'POST');
    assert.equal(req.url, '/v1/responses');
    assert.equal(req.headers.authorization, `Bearer ${apiKey}`);
    assert.equal(req.headers['content-type'], 'application/json');
    let raw = '';
    for await (const chunk of req) raw += chunk;
    assert.equal(raw.includes(apiKey), false);
    const body = JSON.parse(raw);
    assert.deepEqual(Object.keys(body).sort(), ['input', 'model', 'stream']);
    assert.equal(body.model, 'gpt-6-sol');
    assert.equal(body.stream, true);
    assert.equal(body.input.length, 1);
    assert.equal(body.input[0].role, 'user');
    assert.equal(body.input[0].content[0].type, 'input_text');
    assert.ok(body.input[0].content[0].text.includes('不可推断默认值'));
    assert.ok(body.input[0].content[0].text.includes(request('').instruction));
    output(res, completed(JSON.stringify(plan)));
  }, async endpoint => {
    const result = await planParameters(request(endpoint));
    assert.equal(result.ok, true);
    assert.deepEqual(JSON.parse(JSON.stringify(result.plan)), plan);
    assert.equal(JSON.stringify(result).includes(apiKey), false);
  });
});

test('accepts chunked UTF-8 SSE only after response.completed and matching final text', async () => {
  const text = JSON.stringify(plan);
  await withServer((req, res) => {
    res.writeHead(200, { 'content-type': 'text/event-stream' });
    const events = [{ type: 'response.created', response: { status: 'in_progress' } },
      { type: 'response.output_text.delta', delta: text.slice(0, 41) },
      { type: 'response.output_text.delta', delta: text.slice(41) },
      { type: 'response.completed', response: completed(text) }];
    const bytes = Buffer.from(`: comment\r\n\r\n${events.map(event => `event: ${event.type}\r\ndata: ${JSON.stringify(event)}\r\n\r\n`).join('')}`);
    for (let start = 0; start < bytes.length; start += 7) res.write(bytes.subarray(start, start + 7));
    res.end();
  }, async endpoint => {
    const result = await planParameters(request(endpoint));
    assert.equal(result.ok, true, JSON.stringify(result));
    assert.deepEqual(JSON.parse(JSON.stringify(result.plan)), plan);
  });
});

test('tolerates one wholly fenced JSON block', () => {
  assert.deepEqual(JSON.parse(JSON.stringify(parseModelPlan(`\`\`\`json\n${JSON.stringify(plan)}\n\`\`\``))), plan);
});

for (const [name, value] of [
  ['unknown plan key', { ...plan, run: true }],
  ['empty plan', { summary: '无', commands: [] }],
  ['unknown command key', { summary: '设置', commands: [{ ...plan.commands[0], save: true }] }],
  ['unknown page', { summary: '设置', commands: [{ page: '初始化', values: { '马赫数': 0.8 } }] }],
  ['nonstring page', { summary: '设置', commands: [{ page: ['参考量'], values: { '马赫数': 0.8 } }] }],
  ['unknown field', { summary: '设置', commands: [{ page: '参考量', values: { '温度': 288.15 } }] }],
  ['unknown GPU option', { summary: '设置', commands: [{ page: '计算控制', values: { 'GPU 计算': true } }] }],
  ['fractional process count', { summary: '设置', commands: [{ page: '计算控制', values: { '计算进程数': 6.1 } }] }],
  ['out of range', { summary: '设置', commands: [{ page: '参考量', values: { '攻角 (°)': 181 } }] }],
  ['subprecision number', { summary: '设置', commands: [{ page: '参考量', values: { '马赫数': 1e-10 } }] }],
  ['string number', { summary: '设置', commands: [{ page: '参考量', values: { '马赫数': '0.8' } }] }],
  ['duplicate page', { ...plan, commands: [plan.commands[0], plan.commands[0]] }],
  ['partial valid mixed invalid plan', { ...plan, commands: [plan.commands[0], { page: '参考量', values: { '温度': 300 } }] }],
  ['clarification with partial commands', { needs_clarification: true, message: '请补充', commands: plan.commands }],
]) {
  test(`rejects ${name} before any execution`, () => {
    assert.throws(() => parseModelPlan(JSON.stringify(value)), error => error.code === 'INVALID_MODEL_PLAN');
  });
}

for (const [name, raw] of [
  ['prose', `这是计划：${JSON.stringify(plan)}`],
  ['truncated JSON', JSON.stringify(plan).slice(0, -1)],
  ['extra fenced prose', `\`\`\`json\n${JSON.stringify(plan)}\n\`\`\`\n已完成`],
  ['duplicate field', '{"summary":"设置","commands":[{"page":"参考量","values":{"马赫数":0.8,"马赫数":0.9}}]}'],
  ['duplicate escaped field', '{"summary":"设置","commands":[{"page":"参考量","values":{"马赫数":0.8,"\\u9a6c赫数":0.9}}]}'],
  ['lost float precision', '{"summary":"设置","commands":[{"page":"参考量","values":{"马赫数":1.00000000000000001}}]}'],
  ['array root', JSON.stringify([plan])],
  ['nonJSON whitespace', '{\u00a0"summary":"设置","commands":[]}'],
]) test(`rejects ${name}`, () => assert.throws(() => parseModelPlan(raw), error => error.code === 'INVALID_MODEL_PLAN'));

test('all shared fields validate at their boundaries and preserve existing schemas', () => {
  assert.deepEqual(Object.keys(parameterSchemas), ['计算控制', '参考量']);
  for (const [page, fields] of Object.entries(parameterSchemas)) {
    for (const [field, spec] of Object.entries(fields)) {
      const values = spec.role === 'incrementor' ? [spec.min, spec.max] : spec.choices;
      for (const value of values) assert.equal(validateParameterCommand({ page, values: { [field]: value } }).values[field], value);
    }
  }
});

test('clarification is returned as a sanitized nonexecuting result', async () => {
  await withServer((req, res) => output(res, completed(JSON.stringify({ needs_clarification: true, message: `请补充马赫数 ${apiKey}` }))), async endpoint => {
    const result = await planParameters(request(endpoint));
    assert.equal(result.ok, false);
    assert.equal(result.error.code, 'NEEDS_CLARIFICATION');
    assert.ok(result.error.message.includes('请补充马赫数'));
    assert.equal(JSON.stringify(result).includes(apiKey), false);
    assert.equal(Object.hasOwn(result, 'plan'), false);
  });
});

test('redacts model summary and server errors without ever echoing response bodies', async () => {
  await withServer((req, res) => output(res, completed(JSON.stringify({ ...plan, summary: `设置参数 ${apiKey} Bearer other-secret-value` }))), async endpoint => {
    const result = await planParameters(request(endpoint));
    assert.equal(result.ok, true);
    assert.equal(JSON.stringify(result).includes(apiKey), false);
    assert.equal(JSON.stringify(result).includes('other-secret-value'), false);
  });
  await withServer((req, res) => { res.writeHead(401); res.end(`bad key ${apiKey}`); }, async endpoint => {
    const result = await planParameters(request(endpoint));
    assert.equal(result.error.code, 'AUTHENTICATION_FAILED');
    assert.equal(JSON.stringify(result).includes(apiKey), false);
    assert.equal(JSON.stringify(result).includes('bad key'), false);
  });
});

for (const [name, response, code] of [
  ['refusal', { ...completed(''), output: [{ type: 'message', role: 'assistant', content: [{ type: 'refusal', refusal: apiKey }] }] }, 'MODEL_REFUSAL'],
  ['incomplete status', { ...completed(JSON.stringify(plan)), status: 'incomplete' }, 'INCOMPLETE_RESPONSE'],
  ['API error', { error: { message: apiKey } }, 'INCOMPLETE_RESPONSE'],
  ['tool call', { ...completed(JSON.stringify(plan)), output: [{ type: 'function_call', arguments: apiKey }] }, 'INVALID_MODEL_RESPONSE'],
  ['invalid JSON text', completed('{badJSON'), 'INVALID_MODEL_PLAN'],
]) test(`rejects ${name} and sanitizes output`, async () => {
  await withServer((req, res) => output(res, response), async endpoint => {
    const result = await planParameters(request(endpoint));
    assert.equal(result.ok, false);
    assert.equal(result.error.code, code);
    assert.equal(JSON.stringify(result).includes(apiKey), false);
  });
});

for (const [name, events, code] of [
  ['truncated SSE', [{ type: 'response.output_text.delta', delta: JSON.stringify(plan) }], 'INCOMPLETE_RESPONSE'],
  ['failed SSE', [{ type: 'response.failed', response: { error: { message: apiKey } } }], 'INCOMPLETE_RESPONSE'],
  ['mismatched final text', [{ type: 'response.output_text.delta', delta: '{}' }, { type: 'response.completed', response: completed(JSON.stringify(plan)) }], 'INVALID_MODEL_RESPONSE'],
]) test(`rejects ${name}`, async () => {
  await withServer((req, res) => {
    res.writeHead(200, { 'content-type': 'text/event-stream' });
    res.end(events.map(event => `data: ${JSON.stringify(event)}\n\n`).join(''));
  }, async endpoint => {
    const result = await planParameters(request(endpoint));
    assert.equal(result.ok, false);
    assert.equal(result.error.code, code);
    assert.equal(JSON.stringify(result).includes(apiKey), false);
  });
});

test('rejects redirects rather than sending credentials to another endpoint', async () => {
  let targetCalled = false;
  await withServer((req, res) => { targetCalled = true; output(res, completed(JSON.stringify(plan))); }, async target => {
    await withServer((req, res) => { res.writeHead(307, { location: `${target}/responses` }); res.end(); }, async endpoint => {
      const result = await planParameters(request(endpoint));
      assert.equal(result.ok, false);
      assert.equal(result.error.code, 'MODEL_CONNECTION_FAILED');
      assert.equal(JSON.stringify(result).includes(apiKey), false);
    });
  });
  assert.equal(targetCalled, false);
});

test('times out during streaming and aborts an in-flight request on cancellation', async () => {
  await withServer((req, res) => { res.writeHead(200, { 'content-type': 'text/event-stream' }); res.write(': waiting\n\n'); }, async endpoint => {
    const timed = await planParameters(request(endpoint), { timeoutMs: 35 });
    assert.equal(timed.ok, false);
    assert.equal(timed.error.code, 'MODEL_TIMEOUT');
    const controller = new AbortController();
    const pending = planParameters(request(endpoint), { signal: controller.signal, timeoutMs: 3000 });
    setTimeout(() => controller.abort(), 35);
    const cancelled = await pending;
    assert.equal(cancelled.ok, false);
    assert.equal(cancelled.error.code, 'CANCELLED');
  });
});

test('invalid request is rejected before fetch and never includes its secrets', async () => {
  let called = false;
  for (const value of [{ ...request('https://example.invalid/v1'), extra: apiKey },
    request(`https://example.invalid/v1/${apiKey}`),
    { ...request('https://example.invalid/v1'), model: 'bad\nmodel' },
    { ...request('https://example.invalid/v1'), instruction: '' },
    { ...request('https://example.invalid/v1'), apiKey: `${apiKey}\nHeader: injected` }]) {
    const result = await planParameters(value, { fetchImpl: () => { called = true; throw Error(apiKey); } });
    assert.equal(result.ok, false);
    assert.equal(JSON.stringify(result).includes(apiKey), false);
  }
  assert.equal(called, false);
});

test('CLI accepts only stdin, reports sanitized JSON and no stderr', async () => {
  const child = spawn(process.execPath, [fileURLToPath(new URL('./ai-plan.mjs', import.meta.url))], { windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] });
  let stdout = '';
  let stderr = '';
  child.stdout.on('data', value => { stdout += value; });
  child.stderr.on('data', value => { stderr += value; });
  child.stdin.end(JSON.stringify({ ...request('http://remote.invalid/v1'), apiKey }));
  const exit = await new Promise(resolve => child.once('exit', resolve));
  assert.equal(exit, 1);
  assert.equal(stderr, '');
  assert.equal(JSON.parse(stdout).error.code, 'INVALID_ENDPOINT');
  assert.equal(stdout.includes(apiKey), false);
});

test('CLI obtains a complete plan through its stdin protocol', async () => {
  await withServer((req, res) => output(res, completed(JSON.stringify(plan))), async endpoint => {
    const child = spawn(process.execPath, [fileURLToPath(new URL('./ai-plan.mjs', import.meta.url))], { windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', value => { stdout += value; });
    child.stderr.on('data', value => { stderr += value; });
    child.stdin.end(JSON.stringify(request(endpoint)));
    const exit = await new Promise(resolve => child.once('exit', resolve));
    assert.equal(exit, 0, stderr);
    assert.deepEqual(JSON.parse(stdout), { ok: true, plan });
    assert.equal(stderr, '');
    assert.equal(stdout.includes(apiKey), false);
  });
});

test('CLI exits promptly after HTTP 401 even when its response body never ends', async () => {
  await withServer((req, res) => {
    res.writeHead(401, { 'content-type': 'text/plain' });
    res.write(`Authorization denied for ${apiKey}`);
    // Deliberately keep the body open: the client must cancel it after the headers.
  }, async endpoint => {
    const child = spawn(process.execPath, [fileURLToPath(new URL('./ai-plan.mjs', import.meta.url))], { windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', value => { stdout += value; });
    child.stderr.on('data', value => { stderr += value; });
    const closed = new Promise(resolve => child.once('close', code => resolve({ code })));
    let watchdog;
    try {
      child.stdin.end(JSON.stringify(request(endpoint)));
      const result = await Promise.race([closed,
        new Promise(resolve => { watchdog = setTimeout(() => resolve({ timedOut: true }), 2000); })]);
      assert.equal(result.timedOut, undefined, 'CLI stayed alive after reporting the HTTP authentication error');
      assert.equal(result.code, 1, stderr);
      assert.equal(JSON.parse(stdout).error.code, 'AUTHENTICATION_FAILED');
      assert.equal(stdout.includes(apiKey), false);
      assert.equal(stderr, '');
    } finally {
      clearTimeout(watchdog);
      if (child.exitCode === null) child.kill();
      await closed;
    }
  });
});
