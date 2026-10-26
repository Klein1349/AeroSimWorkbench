# 正式模块接入区

本目录包含已接入 GUI 的 `ui/` 主窗口库 `aerosim_ui`；其余子目录仍为业务模块预留区，运行与工作流服务仍使用 `mocks/`。新增模块在相应目录添加 `CMakeLists.txt`、`include/aerosim/<module>/`、`src/` 和必要的 `tests/`，先完成独立目标及自测，再接入顶层构建和应用装配。

公共数据与端口统一放在 `contracts/`，模块不得复制公共类型。服务模块不依赖 `QWidget`；VTK 显示控件、结果读取适配和后处理服务可在 `modules/visualization/` 内组织，但须保持 UI、Service、Infrastructure 边界。Qt/VTK/SARibbon 等依赖由实际使用它们的目标声明，不使 CLI 被 GUI 依赖阻断。
