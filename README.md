# AeroSimWorkbench

本仓库当前包含 C++/Qt 主工程骨架。正式后处理技术路线为 **C++、Qt 5.12.12、VTK 8.2.0**；后处理工作台已提供数据/场景面板、属性面板、时间轴和可选的 VTK Viewer，数据读取与正式场景服务仍待接入。

目录按能力组织：`app/` 放 GUI/CLI 入口与装配，`contracts/` 放公共契约，`modules/` 放正式业务模块，`mocks/` 放当前占位实现，`tests/fixtures/postprocess/` 放小型解析测试数据。详见[文档导航](docs/README.md)和[目录与接入说明](docs/phase1-integration.md)。

默认打开 SARibbon 主窗口；“后处理 → 后处理工作台”打开已有工作台，或使用 `aerosim_gui --postprocess` 直接进入。详见 [UI 模块](modules/ui/README.md)。

## 快速开始

统一开发基线为 C++14、CMake 3.18.2、Qt 5.12.12、Windows VS2017 v141 x64 **Debug**。GUI 查找 Qt Core/Widgets/Svg 和 SARibbon 2.9.3；CLI 仅需 Qt Core；启用可视化模块后才查找 VTK。目标机 VTK 8.2.0 仅提供 Debug 导出，因此完整 C++ 技术栈只在 Debug 配置下验证。环境配置和 Linux 模板见[开发环境说明](docs/DevelopmentEnvironment.md)，代码规范见[开发规范](docs/DevelopmentGuide.md)。

Windows（VS2017 开发者命令提示符）：

```bat
set "AEROSIM_QT_ROOT=C:\Qt\5.12.12\msvc2017_64"
set "AEROSIM_SARIBBON_SOURCE_DIR=<本机 SARibbon 2.9.3 源码目录>"
scripts\build-windows.cmd all Debug
build-windows\Debug\aerosim_cli.exe --mock-once
build-windows\Debug\aerosim_gui.exe
```

启用真实 VTK 视口（目标机只有 Debug VTK 导出）：

```bat
set "AEROSIM_VTK_DIR=D:\Project\VTK\lib\cmake\vtk-8.2"
set "AEROSIM_VTK_BIN=D:\Project\VTK\bin"
set "AEROSIM_BUILD_VISUALIZATION=ON"
set "AEROSIM_BUILD_DIR=build-postprocess"
scripts\build-windows.cmd all Debug
set "AEROSIM_ENABLE_VTK_VIEWER=1"
build-postprocess\Debug\aerosim_gui.exe --postprocess
```

Linux（需自行准备同版本 Qt；当前尚未实机验证）：

```bash
export AEROSIM_QT_ROOT=/path/to/qt-5.12.12
export AEROSIM_SARIBBON_SOURCE_DIR=/path/to/SARibbon-2.9.3
./scripts/build-linux.sh all
./build-linux/aerosim_cli --mock-once
```

无界面构建设置 `AEROSIM_BUILD_GUI=OFF`；仅编译某个当前模块时将 `all` 换为 `aerosim_contracts`、`aerosim_application`、`aerosim_mock` 或 `aerosim_ui`。CMake 3.18 不支持 Presets，因此统一使用脚本。GUI 必须提供 SARibbon 2.9.3 源码；VTK 仅在开启可视化或技术栈探针时查找。无界面构建不依赖 SARibbon。空间特征模块的独立测试见 [modules/visualization/tests/README.md](modules/visualization/tests/README.md)。

## 模块边界

```text
GUI / CLI entry ──> ApplicationController ──> contracts <── Mock services
      │                      │                    ▲
      └── MainWindow / PostProcessWorkbenchWindow ──> UI integration ports ────┘

正式接入：GUI → workflow/project services → visualization/configuration/runtime services → adapters
```

`ApplicationController` 只拥有服务对象并建立工作流服务与任务服务之间的排队连接，不执行任何工作流判断或任务调度。GUI/CLI 都使用 `IWorkflowService` 和 `ITaskService`。跨线程消息按值传递，Qt 自定义类型在连接前注册；界面必须留在 GUI 线程。Mock 任务不会启动求解器，也不持久化记录。

未启用 `AEROSIM_BUILD_VISUALIZATION` 时，GUI 中央视口显示明确标记的占位画布；编译启用后还需设置 `AEROSIM_ENABLE_VTK_VIEWER=1`，才在后处理工作台嵌入 `QVTKOpenGLNativeWidget` Viewer。`aerosim_gui --smoke` 仅验证 Qt 装配与 Mock 任务通信。

详细契约、目录结构、接入方法和未完成工作见 [第一阶段集成说明](docs/phase1-integration.md)；实际测试结果见 [构建与运行记录](docs/phase1-validation.md)。Windows 运行目录的打包与启动步骤见 [部署说明](docs/windows-deployment.md)。

## 后处理集成准备

后处理的 16 项功能大多仍在开发；④⑤⑥⑭ 已作为不带界面的空间特征算法与 VTP 导出交付，尚未接到 Viewer。现有[总体接入方案](docs/PostProcessIntegrationGuide.md)、[公共数据与场景契约草案](docs/PostProcessContracts.md)、[功能交接表](docs/PostProcessTaskHandoff.md)及[解析测试数据](docs/PostProcessTestData.md)。`contracts/` 目前仅补充身份引用类型；场景操作、网格数组和跨模块算法接口仍待评审。执行 `cmake -P tests/fixtures/postprocess/VerifyFixtures.cmake` 可不依赖 Python 检查已提交夹具；正式 C++/VTK 8.2 数据读取仍需模块级验证。
