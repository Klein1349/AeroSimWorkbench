// Shared allowlist for model plans and desktop presets. It mirrors the existing Qt editors.
const number = (min, max, integer = false) => Object.freeze({ role: 'incrementor', min, max, integer });
export const parameterSchemas = Object.freeze({
  '计算控制': Object.freeze({
    '一阶迭代步数': number(0, 1e9, true),
    '最大迭代步数': number(1, 1e9, true),
    '收敛残差': number(-1e9, 1e9),
    '残差监控阈值': number(0, 1e9),
    '计算进程数': number(1, 65536, true),
    'GPU 计算': Object.freeze({ role: 'combobox', choices: Object.freeze(['不使用GPU计算', '使用GPU计算']) }),
  }),
  '参考量': Object.freeze({
    '力矩参考点 X': number(-1e9, 1e9),
    '力矩参考点 Y': number(-1e9, 1e9),
    '力矩参考点 Z': number(-1e9, 1e9),
    '气动弦长 (m)': number(0.000001, 1e9),
    '气动展长 (m)': number(0.000001, 1e9),
    '参考面积 (m²)': number(0.000001, 1e12),
    '马赫数': number(0, 1e4),
    '攻角 (°)': number(-180, 180),
    '侧滑角 (°)': number(-180, 180),
  }),
});

export class ParameterSchemaError extends Error {
  constructor(code, message) { super(message); this.code = code; }
}
const fail = (code, message) => { throw new ParameterSchemaError(code, message); };
const object = value => value !== null && typeof value === 'object' && !Array.isArray(value);

// Returns the original command on success; callers should not execute any command until all validate.
export function validateParameterCommand(value) {
  if (!object(value) || Object.keys(value).length !== 2
      || !Object.hasOwn(value, 'page') || !Object.hasOwn(value, 'values')) {
    fail('INVALID_PRESET', 'Preset must contain only page and values.');
  }
  if (typeof value.page !== 'string' || !Object.hasOwn(parameterSchemas, value.page)) fail('UNSUPPORTED_PAGE', 'Unsupported parameter page.');
  if (!object(value.values) || Object.keys(value.values).length === 0) fail('INVALID_PRESET', 'values must be a nonempty object.');
  for (const [name, requested] of Object.entries(value.values)) {
    if (!Object.hasOwn(parameterSchemas[value.page], name)) fail('UNSUPPORTED_FIELD', 'Unsupported parameter field.');
    const spec = parameterSchemas[value.page][name];
    if (spec.role === 'incrementor') {
      if (typeof requested !== 'number' || !Number.isFinite(requested) || requested < spec.min || requested > spec.max
          || (spec.integer ? !Number.isInteger(requested) : Number(requested.toFixed(6)) !== requested)) {
        fail('INVALID_VALUE', 'Parameter value is outside its range or supported precision.');
      }
    } else if (!spec.choices.includes(requested)) fail('INVALID_VALUE', 'Unsupported parameter option.');
  }
  return value;
}
