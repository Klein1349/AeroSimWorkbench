# 主界面

`aerosim_ui` 是独立的 C++14 / Qt 5.12.12 / SARibbon 2.9.3 主窗口库，GUI 入口已从 `MockWindow` 切换到 `aerosim::MainWindow`。遵循《框架设计与技术调研.统一修改版》的五区布局。

- 顶部九类 Ribbon：工程、前处理、状态管理、求解、监控、后处理、数据处理、工具、帮助。
- 左侧工程树 / 工作流页签；工程树为明确标记的结构示例，WorkflowView 来自服务。
- 中央视口插槽和待接入的视图控制工具栏；不伪造 VTK 渲染。
- 右侧属性草稿编辑器，工程树或 Ribbon 可切换页面。数值变化发出模块内 `parameterDraftChanged` 信号；当前草稿按页面保存，尚未实现逐对象参数模型、专业校验或文件持久化。
- 底部日志、计算监控、收敛曲线占位、错误信息、任务列表；任务按 ID 更新，日志最多 2000 行，错误最多 1000 行，任务最多显示 500 条。
- 面板支持停靠、浮动、关闭和工具菜单恢复。状态栏明确标出未接入能力。

未实现的工程保存、网格导入、后处理和暂停/续算等操作禁用并提供提示；配置入口仅打开草稿。收敛曲线和 CPU/MPI 等指标不生成虚构数据。

## 接入边界

主窗口实现已有 `IWidgetHost`。Viewport、Log、Error、Monitor 各接受一次组件插入，成功后 Qt 父对象管理组件；失败不转移所有权。必须在 GUI 线程插入。原内置显示隐藏但保留，避免排队事件访问已释放控件。已插入组件若被销毁，可以重新插入。

`bindServices` 允许重复绑定和解除绑定，不重复连接。事件使用 `Qt::QueuedConnection`。`view()`、`requestRun()` 是同步契约，只在服务与窗口同线程时调用；服务内部应异步执行耗时任务。

`setRunContext` 接收服务提供的工况及参数快照，不从 UI 草稿构造已校验快照。当前 UI 仅识别 actionId 为 `run` 的运行操作（模块接入约定，不修改公共契约）；服务未提供此操作时保持禁用。不同工况及旧版本视图不覆盖当前已绑定工况。步骤、操作、问题可激活并发出 `navigationRequested(pageId, objectId, fieldKey)`；专业模块负责具体字段定位。

## 构建

GUI 现在必须显式提供 SARibbon 2.9.3 源码，无自动下载。库来源为 https://github.com/czyt1988/SARibbon ，MIT 许可证；沿用已批准的 2.9.3 基线，静态编译且关闭 FramelessHelper。无需 VTK 即可构建窗口。

Windows 主工程：

```bat
set "AEROSIM_QT_ROOT=C:\Qt\5.12.12\msvc2017_64"
set "AEROSIM_SARIBBON_SOURCE_DIR=<本机 SARibbon 2.9.3 源码目录>"
scripts\build-windows.cmd all Debug
build-windows\Debug\aerosim_gui.exe
```

独立模块（路径由本机传入）：

```sh
cmake -S modules/ui -B build-ui -DCMAKE_PREFIX_PATH=<Qt目录> -DSARIBBON_SOURCE_DIR=<SARibbon源码目录>
cmake --build build-ui --config Debug
cd build-ui
ctest -C Debug --output-on-failure
```

无界面构建仍使用 `AEROSIM_BUILD_GUI=OFF`，不依赖 SARibbon / Widgets / VTK。截图命令：`aerosim_gui --screenshot <输出PNG路径>`，保存窗口截图后退出。当前验证范围见 [主界面验证记录](../../docs/ui-validation.md)。

## 合并后的后处理工作台


`aerosim_ui` 提供后处理工作台 `PostProcessWorkbenchWindow`。窗口实现
`aerosim/ui/IntegrationPorts.h` 中的 `IWidgetHost`，由 `app/` 装配工作流和任务
服务；工作流规则、工程持久化、数据读取和 VTK 场景实现均不在本模块。

当前工作台包含：

- 顶部 Tecplot 风格的菜单与快捷工具栏：菜单按“数据、显示、特征、分析、工具”归类，工具栏保留结果导入/导出、网格/云图、切面/等值面/流线/轨迹、视图、探针与 DeepSeek Harness 等高频操作；
- 左侧“数据”和“场景”标签页，分别显示数据集/字段/时间帧以及场景对象层级；
- 中央三维视口插槽；未接入时显示占位画布，正式 Viewer 以 `WidgetSlot::Viewport` 注入；
- 右侧属性面板，用于呈现对象、显示和特征参数；
- 底部时间轴和可展开的消息面板。

工具栏当前只发送或记录操作请求，不伪造尚未实现的数据、特征或渲染行为。实际
VTK Viewer、统一场景入口和异步数据读取应分别由后续 `modules/visualization/scene`
与数据服务提供；UI 不直接持有 renderer、actor 或 VTK 数据对象。

`aerosim_postprocess_ui_smoke` 验证核心控件以及视口插槽只能接受一个 Viewer。

后处理窗口源码位于 `include/aerosim/ui/postprocess/` 与 `src/postprocess/`，测试位于
`tests/postprocess/`。开发期的 Mock 入口只用于切换 P0/P1 的会话初始状态；它明确不
承担数据读取、VTK 算法或场景管理，真实工程选择服务接入后将替换该入口。

默认启动 Ribbon 主窗口，选择“后处理 → 后处理工作台”打开同一进程中的独立工作台窗口（重复点击复用实例）；也可运行 `aerosim_gui --postprocess` 直接打开。工作台的平铺数据/场景面板、时间轴、属性面板及 DeepSeek Harness 入口保留。两个窗口由 GUI 入口绑定同一组服务，主窗口不持有 VTK 数据。

真实 Viewer 将由后续 `modules/visualization/scene` 实现后经 `app/` 注入工作台；当前工作台始终使用明确标记的视口占位。`--screenshot` 可与 `--postprocess` 组合检查工作台。
