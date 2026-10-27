# 正式模块接入区

本目录预留正式业务模块。`modules/visualization/` 当前是按 data、scene、features、analysis 划分的后处理逻辑骨架；`modules/ui/` 已提供两种窗口，其余业务模块仍只有说明，运行与工作流服务仍使用 `mocks/`。新增模块在相应目录添加 `CMakeLists.txt`、`include/aerosim/<module>/`、`src/` 和必要的 `tests/`，先完成独立目标及自测，再接入顶层构建和应用装配。

公共数据与端口统一放在 `contracts/`，模块不得复制公共类型。服务模块不依赖 `QWidget`；VTK 显示控件、结果读取适配和后处理服务可在 `modules/visualization/` 内组织，但须保持 UI、Service、Infrastructure 边界。Qt/VTK/SARibbon 等依赖由实际使用它们的目标声明，不使 CLI 被 GUI 依赖阻断。

`ui/` 的 `aerosim_ui` 同时构建 SARibbon 主窗口和后处理工作台，并分别保留测试；默认 GUI 入口为 Ribbon 主窗口。
