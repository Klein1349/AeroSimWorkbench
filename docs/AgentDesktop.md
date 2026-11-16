# 使用 agent-desktop 配置参数

本项目使用 [Vercel Labs agent-desktop](https://github.com/vercel-labs/agent-desktop) 操作现有 Qt 参数界面。依赖固定为 **1.3.1**，来源为 npm 包 `agent-desktop`，许可证为 Apache-2.0；版本由 `tools/agent-desktop/package-lock.json` 锁定。工具在独立进程中使用 Windows UI Automation，不链接到 Qt/VTK，也不改变现有 C++ 工具链。

窗口内的“参数助手”默认使用 AI 直接调用 agent-desktop，当前接入 RightAPI 的 OpenAI Responses 工具调用接口。AI 读取当前工作台、选择桌面工具、执行操作，再根据返回的界面状态继续操作和核对结果。AI 模式没有参数名、页面数或参数页面白名单；控制方程、边界条件、网格运动、求解方法、参考量、输出控制、初始化、计算控制及其弹窗均通过现有界面操作。

原有本地规则解析保留为用户显式选择的模式。AI 请求失败或尚未填写密钥时会提示原因，不会自动改用本地模式执行。

**参数页当前仍写入窗口草稿。** AI 使用应用已有功能，原有业务服务、工程保存和求解器是否接入不会因 AI 改变。配置参数不隐含启动计算，现有按钮的启用状态、数值范围及“确定/取消”语义继续生效。

## 安装与启动

本接入使用 Windows 交互桌面会话，以及 PATH 中的 Node.js 18 或更新版本、npm。当前 Qt GUI 的验证基线仍是 Windows x64 / VS2017 / Qt 5.12.12 Debug；工具包另有 Windows ARM64 二进制，不代表本项目 GUI 已通过 ARM64 验证。

在仓库根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/setup-agent-desktop.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/start-parameter-workbench.ps1
```

安装脚本运行 `npm ci` 和 `status` 检查，并为当前检出目录生成 `.codex/config.toml` 中的 `aerosim-desktop` MCP 配置；已有同名配置会保留。依赖目录、截图产物和本机 MCP 路径不提交到 Git。仅安装命令行工具时可给安装脚本加 `-SkipMcp`。

启动脚本默认打开 `build-windows/Debug/aerosim_gui.exe --workbench`，启用 Qt 无障碍支持；同一路径已有主窗口时会保留正在运行的窗口及其草稿。使用其他构建目录时传入 `-Executable`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/start-parameter-workbench.ps1 -Executable "C:/path/to/build/Debug/aerosim_gui.exe"
```

若尚未构建 GUI，先按[开发环境说明](DevelopmentEnvironment.md)配置本机依赖，再执行 `scripts/build-windows.cmd aerosim_gui Debug`。安装 agent-desktop 本身不会编译主程序。

## 配置 RightAPI

参数设置总览及各参数页底部常驻紧凑的“参数助手”栏。打开“助手设置 → AI 设置…”，填写并保存：

| 项目 | 填写内容 |
| --- | --- |
| 接口地址（Base URL） | 默认 `https://api.site/v1`，可改为提供 Responses 协议的接口地址 |
| 模型 | 默认 `gpt-6-sol`，这是 RightAPI Curl 文档示例；可改为当前 Key 绑定渠道实际支持的模型名 |
| API 密钥 | 在本机密码输入框填写 RightAPI Key |

RightAPI 使用 `Authorization: Bearer` 鉴权；应用向 Base URL 对应的 `/responses` 发出请求。地址与模型保存在本机设置中，Windows 下密钥保存在凭据管理器，不写入源码、仓库配置或命令行参数。“显示密钥”用于主动查看输入，“移除已保存密钥”在保存时删除本机凭据。应用退出后重新打开仍可读取已保存配置。

RightAPI 各渠道共用地址，**使用哪个渠道由 Key 的绑定决定**；填写文档示例模型不保证该 Key 有权限。请从提供商后台模型列表复制可用的模型 ID。`401` 常见于密钥问题，`model not found` 可能是模型名不符或渠道无权限。设置保存成功只代表本机配置已保存，真实连接和模型权限以实际请求结果为准。

分离部署或自动化环境也可提供 `AEROSIM_AI_BASE_URL`、`AEROSIM_AI_MODEL`、`AEROSIM_AI_API_KEY`。读取优先使用已保存地址、模型和 Windows 凭据，缺失时才读取对应环境变量；因此删除本机凭据后，环境变量仍可能提供密钥。普通使用建议直接在“AI 设置…”中填写。

本接入选择 Codex / Responses 协议，使用 `https://api.site/v1`，模型需支持 function calling。RightAPI 的 Claude 地址 `https://api.site` 和 Gemini 地址 `https://api.site/v1beta/models` 属于其他协议，不能直接填入当前助手使用。

文档依据：[Curl 调用示例](https://docs.rightapi.io/docs/rc_extension/curl.html)、[渠道地址速查](https://docs.rightapi.io/docs/rc_questions/endpoints.html)、[渠道与模型](https://docs.rightapi.io/docs/rc_quick_start/models.html)。RightAPI 明确说明 `/chat/completions` 由 Responses 转换，可能有兼容问题，并会替换 system prompt；本接入直接使用 Responses。

## 窗口内参数助手

在“助手设置”中选择 AI 桌面模式（默认），输入中文请求后点击“执行指令”。AI 通过 MCP 获取桌面工具定义，在 Responses function calling 循环中将工具返回交回模型，直到完成任务或说明需要补充的信息；执行过程可以直接修改界面。“使用说明”介绍操作方式，示例菜单可填入文字。例如：

```text
将输出控制的续算文件输出间隔改为200，并核对结果
打开初始化设置，读取当前全局初始化参数，暂时不要修改
将马赫数设为0.8、攻角设为3度，同时把最大迭代步数设为12000
查看第一个运动体的参数设置，说明当前使用的运动类型
```

桌面工具负责观察、点击、输入、选择、键鼠交互和窗口操作；AI 根据实时可见控件处理字段和弹窗，不通过旧的两页参数结构执行。运行器绑定当前工作台进程及其所属窗口，其他应用和系统操作不属于本项目的任务范围。AI 设置中的凭据不供桌面助手读取。

“执行日志”入口展示任务进度、AI 的操作说明、工具名称与输入、工具返回和最终结果。日志停靠在工作台底部，可浮动或关闭；运行中可打开查看，支持清空和复制，容量有界，使用纯文本显示。日志不包含 API Key，也不展示模型内部推理。点击“停止”或按 `Ctrl+Shift+F12` 会中止后续工具调用；快捷键在参数模态弹窗打开时也可使用，已经完成的操作保留。

使用界面标明的单位和数值语义。例如“收敛残差”的 `-10` 表示输入控件里的 `-10`；不要求 AI 自行选择物理模型或猜测没有给出的参数。

AI 会收到本次指令、桌面工具定义及完成任务所需的工作台界面观察。助手栏和执行日志从无障碍树中过滤；调用截图工具时，会将工作台当前可见区域（可能含助手栏和已脱敏的日志）发送给所配置的模型。AI 设置窗口禁止观察和截图。API Key 仅用于请求鉴权，不放入模型提示词或日志。助手不会读取整个仓库来获取应用状态。

在“助手设置”中显式选择“本地规则模式”后，可使用原有离线解析。此模式仍限计算控制和常用参考量，每次配置一个页面，字段间用逗号分隔；适合固定表达，例如“最大迭代步数设为12000，进程数6，启用GPU”。此范围只属于离线解析和 JSON 预设工具，不限制 AI 桌面模式。

执行期间避免手动切页或同时修改同一字段。请求和桌面执行均为异步过程；停止不回滚已经写入的值。执行完成后可直接在对应页面检查参数，原有按钮和弹窗继续使用原来的操作方式。

助手从程序目录向上查找本仓库的工具安装，Node.js 优先从 PATH 查找。分离部署时可设置 `AEROSIM_AGENT_DESKTOP_ROOT` 为包含 `tools/agent-desktop` 的仓库根目录，`AEROSIM_NODE_EXECUTABLE` 为 Node 可执行文件的完整路径。内嵌助手始终绑定当前工作台进程；桌面子进程在 Windows Job 中管理，主程序退出后会清理。界面过小时，工具会停止操作被裁切的控件并提示放大或滚动。

## JSON 预设与命令行

`tools/agent-desktop/configure.mjs` 提供可复用的参数预设入口。预设只包含 `page` 和非空 `values`，字段使用界面完整名称，值使用 JSON 数字或选项文字。例如仓库内的 `examples/calculation-control.json`：

```json
{
  "page": "计算控制",
  "values": {
    "最大迭代步数": 12000,
    "计算进程数": 6,
    "GPU 计算": "使用GPU计算"
  }
}
```

先手动打开对应参数页，可进行只读预览；预览不会切页、修改参数或改变焦点：

```powershell
node tools/agent-desktop/configure.mjs tools/agent-desktop/examples/calculation-control.json --dry-run
```

执行预设时使用 `--headed`，允许工具聚焦窗口、点击导航、提交数值编辑，并切离后返回页面核对草稿：

```powershell
node tools/agent-desktop/configure.mjs tools/agent-desktop/examples/calculation-control.json --headed
node tools/agent-desktop/configure.mjs --help
```

成功输出包含 `scope: "window-draft"`、修改前后值和 `verified: true`。失败可能发生在一部分字段已经修改之后；此时先读取输出中的已完成操作、回读值和实际界面，再决定后续处理。它不提供事务回滚，不能把失败结果当作“没有修改”。

## 外部 agent 与 MCP

若需要让 Codex 等外部 agent 根据更自由的描述操作参数，安装脚本可同时注册 Codex CLI 中的同名 MCP 服务：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/setup-agent-desktop.ps1 -RegisterCodex
codex mcp get aerosim-desktop --json
node tools/agent-desktop/check-mcp.mjs
```

`-RegisterCodex` 需要 `codex.cmd` 可用，已有同名服务会保留；`check-mcp.mjs` 验证 stdio 握手、工具清单和只读状态调用，不会修改桌面。项目内配置和 CLI 注册都绑定当前本机安装路径，移动目录后需核对配置中的路径。是否已在当前会话加载，以客户端实际工具清单为准。

也可以直接通过仓库包装命令调用，无需全局安装：

```powershell
scripts/agent-desktop.cmd status
scripts/agent-desktop.cmd skills get desktop
scripts/agent-desktop.cmd skills get platform
scripts/agent-desktop.cmd list-windows --app aerosim_gui.exe
scripts/agent-desktop.cmd snapshot --app aerosim_gui.exe -i --compact
```

操作循环为：读取目标窗口 → 使用本次快照的名称、角色和唯一 ref 定位 → 执行 → 再次读取核验。PowerShell 中必须给 ref 加引号，例如 `scripts/agent-desktop.cmd get '@snapshot-id:e17' --property value`，其中 ref 必须替换为实际快照返回值。UI 变化后重新取快照；不要复用示例 ref 或旧坐标。

外部 agent 能操作的范围取决于控件实际暴露的能力。边界编辑仍需点击“应用”，运动和局部初始化弹窗仍按“确定”写回、“取消”放弃处理。自动化不能绕过这些已有交互，也不能把 UI 范围内的检查视为专业物理校验。

## Qt 5.12 导航兼容性

Qt 5.12.12 的 Windows 无障碍实现会向树项目暴露 InvokePattern，但内部树项目只处理 `toggleAction`，Invoke 发送的 `pressAction` 可能没有效果，即使命令返回成功。因此不能只凭 `click @ref` 的成功状态认定页面已打开。

本项目 Node 工具采用：聚焦绑定的主窗口 → 从最新快照取得导航树项目 bounds → 使用 `--headed` 点击该位置 → 重新读取目标页。数值字段通过语义值接口修改，并移开焦点、切页重开核对，确认值已经进入窗口草稿。GPU下拉框同样使用当前快照中的弹出选项位置选择，并回读核对。UI Automation 返回的 bounds 已是屏幕物理坐标，不应再手工乘 Windows 缩放比例。

遇到页面未打开、找不到字段、窗口不唯一或快照不完整时，先查看当前窗口状态和错误输出。恢复被最小化的主窗口、关闭阻塞弹窗，确保参数树及目标条目可见，再取新快照。不要在不知道先前动作是否落地时连续重复写入。

源码依据：[树项目动作](https://github.com/qt/qtbase/blob/v5.12.12/src/widgets/accessible/itemviews.cpp#L927)、[Invoke 暴露条件](https://github.com/qt/qtbase/blob/v5.12.12/src/plugins/platforms/windows/uiautomation/qwindowsuiamainprovider.cpp#L336)、[Invoke 实现](https://github.com/qt/qtbase/blob/v5.12.12/src/plugins/platforms/windows/uiautomation/qwindowsuiainvokeprovider.cpp#L65)。

## 验证记录

2026-11-14 在加入 AI 规划前，于 Windows x64 / VS2017 v141 / Qt 5.12.12 Debug / Node.js 24.11.1 上验证：全量构建成功，CTest 33项全部通过，`npm test --prefix tools/agent-desktop` 的9项预设拒绝测试通过，MCP握手与75项工具发现成功。通过窗口内输入框实测“最大迭代步数12000，进程数6，启用GPU”和“马赫数0.8，攻角5度”，均在切页后回读验证成功。这些记录验证本地解析及桌面执行链路。

同日在加入 AI 规划后，全量 Debug 构建成功，无编译警告；CTest 36项全部通过，Node 测试52项全部通过。新增测试覆盖 Responses HTTP/SSE 协议、完整响应要求、字段范围和数值精度、认证错误及响应中断、取消请求、密钥脱敏与标准输入传递，以及预览取消不执行、两页按序应用且保持忙碌、AI失败不自动降级。设置测试使用临时 INI 配置，面板测试注入合成配置，不读取或写入用户凭据。已实窗检查助手栏、模式菜单及 AI 设置的布局和默认地址/模型。

随后由用户在应用内自行填写并保存密钥和模型，通过 `https://api.site/v1` 完成一次真实 Responses 请求。指令“把马赫数设为0.8、攻角设为5度，同时将计算进程数设为6，其他参数保持原值”成功生成参考量和计算控制两页、共3项准确数值的方案。取消实窗预览后，马赫数与攻角仍为原值3和0；本次真实请求只验证规划及预览，没有应用测试参数。真实调用不记录密钥；它说明当时保存的渠道与模型可用，其他模型和账户额度仍以各次服务响应为准。

HTTP 模拟测试、方案验证测试和设置界面测试使用本机测试响应或合成凭据，不代表供应商联调；真实调用与桌面执行的验证范围分别以上述记录为准。

同日将 AI 模式切换为直接的 MCP 工具循环后，全量 Debug 构建成功，CTest **37/37**、Node 测试 **78/78** 通过。新增覆盖工具动态发现、任意观察字段的操作、真实本地 HTTP 多轮 function call/output、MCP stdio 握手、截图作为图片回传、弹出界面观察、交付不确定时先观察、进程绑定、取消及子进程清理、日志脱敏和容量上限。面板测试还覆盖模态弹窗中的 `Ctrl+Shift+F12` 停止、底部日志独占整行，以及用户日志布局恢复。

使用用户已保存的 RightAPI 设置完成真实桌面闭环：AI 经 8 次工具调用，将输出控制中的“续算文件输出间隔”从 100 改为 200，Tab 提交后切至初始化页，再返回核对 200，最后还原为 100 并回读；独立桌面观察确认原值已恢复。另为测试临时启用动网格并创建一个默认运动体，AI 通过截图和桌面工具打开“强迫运动参数设置”，读取 `motion_body_1`、强迫运动、无旋转和无平动，再取消弹窗，确认返回原页。测试后已将运动体数目还原为 0，并关闭动网格。两次测试均未启动计算。早期一次请求在完成导航后发生模型连接中断，重试的上述闭环成功；连接异常会停止后续操作并显示脱敏诊断。

这些真实测试覆盖了参数编辑、切页、截图定位及模态弹窗，并不代表每个控件已逐一验收。Qt 表格的嵌入控件可能不在无障碍树中，此时需要截图定位；模型推理和网络等待仍会影响操作耗时。
