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

- 顶部只有一层 SARibbon，保留“数据、显示、特征、分析、工具”分类；“数据”首页在同一 Ribbon 中集成导入/导出、交互、显示、空间特征、视图和查询操作，不再叠加第二条快捷工具栏；
- 左侧“视图 / 数据 / 对象”标签页，依次管理当前 Frame 的全局图层、结果集/Zone/字段/时间帧，以及派生对象层级；
- 中央活动 Frame 视口插槽；未接入时显示明确标记的预览、Frame/字段/时间状态和方向提示，正式 Viewer 以 `WidgetSlot::Viewport` 注入；
- 右侧“属性 / Probe / 任务”检查器：属性随当前对象变化，Probe 只承载拾取结果，任务页呈现后续服务的进度与错误；
- 底部时间轴和可展开的消息面板。

Ribbon 当前只发送或记录操作请求，不伪造尚未实现的数据、特征或渲染行为。实际
VTK Viewer、统一场景入口和异步数据读取应分别由后续 `modules/visualization/scene`
与数据服务提供；UI 不直接持有 renderer、actor 或 VTK 数据对象。

`aerosim_postprocess_ui_smoke` 验证核心控件以及视口插槽只能接受一个 Viewer。

后处理窗口源码位于 `include/aerosim/ui/postprocess/` 与 `src/postprocess/`，测试位于
`tests/postprocess/`。开发期的 Mock 入口只用于切换 P0/P1 的会话初始状态；它明确不
承担数据读取、VTK 算法或场景管理，真实工程选择服务接入后将替换该入口。

默认启动 Ribbon 主窗口，选择“后处理 → 后处理工作台”打开同一进程中的独立工作台窗口（重复点击复用实例）；也可运行 `aerosim_gui --postprocess` 直接打开。工作台的平铺数据/场景面板、时间轴、属性面板及 DeepSeek Harness 入口保留。两个窗口由 GUI 入口绑定同一组服务，主窗口不持有 VTK 数据。

真实 Viewer 将由后续 `modules/visualization/scene` 实现后经 `app/` 注入工作台；当前工作台始终使用明确标记的视口占位。`--screenshot` 可与 `--postprocess` 组合检查工作台。

## 后处理视觉与布局检查

后处理保留自己的三栏内容，以参考主界面的浅灰标题栏、白色 Ribbon、蓝灰工作区、
Microsoft YaHei UI 9pt 和线性 SVG 图标统一视觉。图标通过本模块的
`PostProcessIcons.qrc` 打包，并由 Qt Svg 渲染多种尺寸；SVG 不依赖字库中的装饰符号。
SARibbon 静态资源必须显式注册，否则主题、标题栏图标会退回系统默认样式。

工作区样式单独作用于中央控件，避免输入框和按钮规则污染 Ribbon。检查器采用可滚动页面，
窄窗表单按行排列，视口标题/字段分两行，时间步进使用由图标尺寸决定大小的工具按钮。
附加停靠面板的标题使用布局计算高度，避免 Qt 5.12 原生标题区与 QSS padding 不一致导致裁切。

`aerosim_postprocess_ui_smoke` 除功能检查外，也检查 1120×680、1440×960 下的控件几何、
图标资源、停靠标题及是否意外出现第二层工具栏。设置 `AEROSIM_UI_CAPTURE_DIR` 后可导出
五个 Ribbon 页、各类检查器和 Harness 的整窗截图。Windows 视觉复查应使用
`QT_QPA_PLATFORM=windows`；再设 `QT_SCALE_FACTOR=1.5` 检查 150% 缩放。
CTest 的 offscreen 测试负责装配与几何回归，不代替原生字体截图验收。
