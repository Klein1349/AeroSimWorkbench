# 主界面验证记录 · 2026-10-26

本次实现 `modules/ui` 的 SARibbon 主窗口，并替换 GUI 入口的 MockWindow。业务服务仍为 Mock；不表示真实工程保存、网格读取、VTK 渲染、求解或后处理已经验收。

## 正式基线检查

仓库继续精确要求 Qt 5.12.12、C++14、SARibbon 2.9.3。使用本机 Qt 配置主工程时，CMake 明确拒绝 Qt 5.15.19：缺少精确匹配的 Qt 5.12.12。没有放宽仓库的版本要求。

Windows VS2017 v141 x64 Debug、Qt 5.12.12 环境未在此次验证中运行；应在目标机设置 Qt 与 SARibbon 路径后执行 `scripts\build-windows.cmd all Debug`。VTK 视口仍是插槽，无需链接 VTK。

## 隔离编译与运行检查

在仓库外复制源码，仅将该临时副本的 Qt 查找版本替换为本机 5.15.19，用于发现编译、生命周期与布局问题。使用 macOS、AppleClang 21、C++14、Qt 5.15.19 和 SARibbon 官方 v2.9.3 tag（e5d411d48e4060c7c56033763e79bb0f6c4d7b30），Debug 静态 SARibbon，关闭 FramelessHelper。

| 检查 | 结果 |
| --- | --- |
| 主工程全量编译 | 通过 |
| CLI Mock 异步闭环 | 通过 |
| SARibbon GUI Mock smoke | 通过 |
| 后处理数据夹具完整性 | 通过 |
| UI 插槽所有权、重复插入、重复绑定、运行可用性、任务按 ID 更新、解除服务绑定 | 通过 |
| `modules/ui` 独立配置、编译、CTest | 1/1 通过 |
| GUI=OFF 独立配置、编译、CTest（未设置 SARibbon 路径） | 2/2 通过 |
| 主工程 CTest | 4/4 通过 |
| `git diff --check` | 通过 |

使用 offscreen 平台实际创建 1440×960 主窗口并截图检查：九类 Ribbon、工程树/工作流、视口占位、属性草稿和五类输出页签布局完整。修复了静态链接时 SARibbon 资源未注册，以及 SARibbon 延迟应用主题覆盖本窗口样式的问题。截图可通过 `aerosim_gui --screenshot <输出PNG路径>` 重现。

这组结果仅是 macOS / Qt 5.15 的补充检查，不替代 Qt 5.12.12 / Windows 目标机验收。尚未验证完整 VTK 栈、真实业务、Windows 打包、其他 DPI/屏幕尺寸及长时间高频监控。

## 与远端后处理工作台合并后的复验

合并 `origin/main` 的 `853995c`：保留远端可视化源码和后处理工作台，默认入口使用 Ribbon 主窗口，通过“后处理 → 后处理工作台”或 `--postprocess` 进入原工作台。保留原 VTK 编译选项、运行开关及 smoke 模式禁用 OpenGL 的逻辑；不修改公共契约。

同一隔离 macOS / Qt 5.15.19 环境中重新配置并编译：主工程 CTest **6/6**、UI 独立构建 CTest **2/2**、GUI=OFF CTest **2/2** 通过。新增检查覆盖 Ribbon 打开工作台、关闭后再次打开，并分别运行两个 GUI 入口的 Mock smoke。Linux 构建脚本通过 `bash -n`；Windows 构建脚本保留远端的可视化和 SARibbon 参数。

本次没有 Qt 5.12.12 / VTK 8.2 / VS2017 目标环境，因此未重新验证 Windows 全栈或可视化算法测试，不以补充检查代替正式基线验收。
