# 可视化与后处理逻辑

本目录是从当前后处理工作台重新建设真实逻辑的骨架。它不创建第二套后处理窗口；窗口、Ribbon、停靠面板和交互控件仍在 `modules/ui/`。

当前不含 VTK 场景、结果读取或空间特征的实现，也没有 CMake 构建目标。只有当某个子模块拥有独立实现和测试时，才为它新增目标并在根 `CMakeLists.txt` 中接入。

```text
data/       结果导入、结果集、Zone、边界、字段与时间帧
scene/      VTK 场景、图层、Actor、拾取、相机与派生对象管理
features/   切面、等值线、等值面、流线、迹线与特征导出
analysis/   监控曲线、多状态、公式、表格与 AI
tests/      各子模块的独立测试
```

建议的依赖方向是 `data -> features -> scene`；`analysis` 读取 `data` 的数据与元信息，但不依赖 `scene` 或 QWidget。`app/` 只负责装配，`modules/ui/` 不直接操作 VTK 对象。
