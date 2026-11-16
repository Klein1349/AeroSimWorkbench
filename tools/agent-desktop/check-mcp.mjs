// Check the installed stdio server without invoking desktop mutations.
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { createInterface } from 'node:readline';

const binary = fileURLToPath(new URL(`./node_modules/agent-desktop/bin/agent-desktop-win32-${process.arch}.exe`, import.meta.url));
const child = spawn(binary, ['mcp'], { windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] });
const pending = new Map();
let nextId = 0;
let diagnostics = '';
child.stderr.on('data', data => { diagnostics = (diagnostics + data).slice(-4000); });
createInterface({ input: child.stdout }).on('line', line => {
    let message;
    try { message = JSON.parse(line); } catch { return; }
    const request = pending.get(message.id);
    if (request) {
        pending.delete(message.id);
        message.error ? request.reject(new Error(JSON.stringify(message.error))) : request.resolve(message.result);
    }
});
child.on('error', error => { for (const request of pending.values()) request.reject(error); });
child.on('exit', code => { for (const request of pending.values()) request.reject(new Error(`Server exited (${code}): ${diagnostics}`)); });
function send(method, params, notification = false) {
    if (notification) { child.stdin.write(JSON.stringify({ jsonrpc: '2.0', method, params }) + '\n'); return; }
    const id = ++nextId;
    return new Promise((resolve, reject) => {
        pending.set(id, { resolve, reject });
        child.stdin.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
    });
}
const timeout = setTimeout(() => { console.error('MCP handshake timed out.'); child.kill(); process.exitCode = 1; }, 20000);
try {
    const initialized = await send('initialize', { protocolVersion: '2024-11-05', capabilities: {}, clientInfo: { name: 'aerosim-desktop-check', version: '1' } });
    send('notifications/initialized', {}, true);
    const listing = await send('tools/list', {});
    const statusTool = listing.tools.find(tool => tool.name === 'desktop_status');
    if (!statusTool) throw new Error('desktop_status is missing from MCP tools.');
    const status = await send('tools/call', { name: statusTool.name, arguments: {} });
    if (status.isError) throw new Error(JSON.stringify(status));
    console.log(JSON.stringify({ ok: true, server: initialized.serverInfo, toolCount: listing.tools.length, status }));
} catch (error) {
    console.error(error.message);
    process.exitCode = 1;
} finally {
    clearTimeout(timeout);
    child.kill();
}
