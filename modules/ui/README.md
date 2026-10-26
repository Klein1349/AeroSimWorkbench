# 主界面

`aerosim_ui` 提供后处理工作台 `PostProcessWorkbenchWindow`。窗口实现
`aerosim/ui/IntegrationPorts.h` 中的 `IWidgetHost`，由 `app/` 装配工作流和任务
服务；工作流规则、工程持久化、数据读取和 VTK 场景实现均不在本模块。

当前工作台包含：

- 顶部后处理工具栏和菜单：结果导入、网格/云图、切面、等值面、流线、视图和导出入口；
- 左侧“数据”和“场景”标签页，分别显示数据集/字段/时间帧以及场景对象层级；
- 中央三维视口插槽；未接入时显示占位画布，正式 Viewer 以 `WidgetSlot::Viewport` 注入；
- 右侧属性面板，用于呈现对象、显示和特征参数；
- 底部时间轴和可展开的消息面板。

工具栏当前只发送或记录操作请求，不伪造尚未实现的数据、特征或渲染行为。实际
VTK Viewer、统一场景入口和异步数据读取应分别由后续 `modules/visualization/scene`
与数据服务提供；UI 不直接持有 renderer、actor 或 VTK 数据对象。

`aerosim_postprocess_ui_smoke` 验证核心控件以及视口插槽只能接受一个 Viewer。
