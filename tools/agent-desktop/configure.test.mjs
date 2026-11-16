import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

const helper = fileURLToPath(new URL('./configure.mjs', import.meta.url));

// These rejected requests must exit before window discovery or any desktop action.
for (const [name, preset, code] of [
  ['subprecision value', { page: '参考量', values: { '马赫数': 1e-10 } }, 'INVALID_VALUE'],
  ['rounding required', { page: '参考量', values: { '攻角 (°)': 5.1234567 } }, 'INVALID_VALUE'],
  ['fractional process count', { page: '计算控制', values: { '计算进程数': 6.1 } }, 'INVALID_VALUE'],
  ['out of range', { page: '计算控制', values: { '计算进程数': 65537 } }, 'INVALID_VALUE'],
  ['unknown GPU option', { page: '计算控制', values: { 'GPU 计算': '开启全部' } }, 'INVALID_VALUE'],
  ['unsupported page', { page: '网格运动', values: { '启用': true } }, 'UNSUPPORTED_PAGE'],
  ['unknown operation', { page: '计算控制', values: { '启动计算': true } }, 'UNSUPPORTED_FIELD'],
  ['extra command', { page: '参考量', values: { '马赫数': 0.8 }, run: true }, 'INVALID_PRESET'],
  ['empty request', { page: '参考量', values: {} }, 'INVALID_PRESET'],
]) {
  test(`rejects ${name} before desktop access`, () => {
    const directory = mkdtempSync(join(tmpdir(), 'aerosim-preset-test-'));
    try {
      const path = join(directory, 'preset.json');
      writeFileSync(path, JSON.stringify(preset));
      const child = spawnSync(process.execPath, [helper, path, '--headed'], { encoding: 'utf8', timeout: 5000, windowsHide: true });
      assert.equal(child.status, 1, child.stderr);
      const result = JSON.parse(child.stdout);
      assert.equal(result.ok, false);
      assert.equal(result.error.code, code);
      assert.deepEqual(result.attempted_commands, []);
      assert.deepEqual(result.completed_mutations, []);
      assert.deepEqual(result.before, {});
    } finally {
      rmSync(directory, { recursive: true, force: true });
    }
  });
}
