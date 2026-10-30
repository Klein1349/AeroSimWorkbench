# 后处理界面视觉复验 · 2026-10-30

本次只重构后处理视觉与布局，保留数据、显示、特征、分析、工具分类及三栏职责。
顶部只有一层 Ribbon，原快捷栏操作合并到数据首页的功能组，专用分类仍复用相同 QAction。
不表示真实网格读取、VTK 渲染、分析算法或 Harness 服务已实现。

## 实际环境

Windows 10 x64、VS2017 v141（MSVC 19.16）、Qt 5.12.12、C++14、Debug。
本次远端源码与仓库 `cmake/SARibbonDependency.cmake` 实际标称/要求 **SARibbon 2.9.2**，
与开发文档记录的 2.9.3 不一致；本次没有升级或替换依赖，不将本结果标称为 2.9.3 验证。
未启用 VTK；不涉及 VTK Release 或可视化算法验收。

## 构建与回归

代码验收提交：`edb9e57`（其后的记录提交只更新文档）。

| 检查 | 结果 |
| --- | --- |
| `scripts\build-windows.cmd all Debug`，常用 `build-windows` 目录 | 完成，CTest 6/6 |
| `modules/ui` 独立配置与 Debug 构建 | 完成，CTest 2/2 |
| Windows 原生平台，100% 缩放 | 通过，13 张整窗截图 |
| Windows 原生平台，150% 缩放 | 通过，13 张整窗截图 |
| 部署后的 `build-windows/Debug/aerosim_gui.exe --postprocess --screenshot ...` | 成功，1440×960 整窗复查 |
| `git diff --check` | 通过 |

两档缩放均覆盖 1120×680、1440×960 的五个 Ribbon 分类，左侧视图/数据/对象，
右侧 Zone、字段、派生对象、Probe、任务，以及附加时间、曲线检查器、Harness 停靠面板。
检查包括时间轴图标、可见表单几何、无第二层工具栏、主题与命令图标资源、停靠标题完整性。
窗口尺寸为 Qt 逻辑尺寸，150% 截图的物理像素相应放大。

## 复验中修正的问题

- SARibbon 静态资源未注册导致主题加载失败，出现原生凸起边框、缺失窗口控制图标；
  现显式注册实际存在的资源并在测试中检查主题文件。
- 工作区字体继承不一致、全局按钮样式干扰 Ribbon；现将工作区样式限定在中央区域。
- 固定窄时间轴按钮叠加横向 padding 导致内容不足；现使用图标工具按钮的尺寸提示。
- 视口标题拥挤、窄窗表单拥挤；现采用双行视口标题、纵向表单及可滚动检查器。
- Qt 5.12 原生停靠标题未计入 QSS padding；现用真实标题栏布局，检查字高与宽度。
- 已部署 GUI 的本地平台插件目录缺少 offscreen，两个 GUI CTest 启动失败；
  Windows 构建脚本为测试明确指定匹配 Qt 的平台插件目录，复跑 6/6 通过。
- Windows 脚本在括号块中提前展开 errorlevel，可能把失败 CTest 当成成功；现用延迟展开返回真实结果。
- SARibbon 子项目旧 CMake policy 覆盖独立构建的静态库和关闭示例选项；现通过 CMP0077 保留宿主设置。

复现截图：给 `aerosim_postprocess_ui_smoke.exe` 设置 `QT_QPA_PLATFORM=windows` 和
`AEROSIM_UI_CAPTURE_DIR=<输出目录>`；第二轮再设置 `QT_SCALE_FACTOR=1.5`。
测试程序输出截图后退出，不留下占用正式可执行文件的窗口。
