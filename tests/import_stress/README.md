# 文件导入压力与边界测试

对正式 `BoundaryLoadService` 执行读取、校验、边界提取、共享边构建和数据登记测试。服务层性能计时不包含渲染；`ui_import_only` 仅检查真实文件选择框取消、导入完成和错误恢复，不做视角或显示模式压测。

## 运行

要求 Windows x64、VS2017 v141、Qt 5.12.12、VTK 8.2.0 Debug、SARibbon 2.9.3、CGNS 4.4.0 Debug、Python 3.10+ 和 .NET Framework C# 编译器。

```powershell
./tests/import_stress/run.ps1 -LocalStack '<本机依赖根目录>'
```

依赖根目录采用当前本机依赖布局：`tools/cmake-3.18.2/bin`、`deps/qt/5.12.12/msvc2017_64`、`deps/SARibbon`、`deps/cgns-4.4.0-debug` 和 `v`（VTK 的 CMake 配置及 `bin`）。路径由参数传入，不下载或升级依赖。

默认创建 `build-import-stress`，将生成数据、探针输出和日志写入带时间戳的 `out/import-stress-*`。可用 `-BuildDirectory`、`-OutputDirectory`、`-SamplesDirectory` 指定位置；已有探针可使用 `-SkipBuild`。完整测试会生成总计约 16 GB 的样例，建议预留 20 GB 磁盘空间。生成器在目标文件不存在时才创建文件。

只复核小文件边界、权限和 UI 路径：

```powershell
./tests/import_stress/run.ps1 -LocalStack '<本机依赖根目录>' -Phases boundary,permissions,ui
```

各阶段依次为 `boundary`、`stress`、`permissions`、`supplements`、`ui`、`larger`、`recheck`。权限测试只修改新生成的专用夹具，并在 `finally` 中恢复 ACL。`supplements` 需要 `boundary` 先生成最小夹具，`recheck` 需要 `supplements` 的异常头清单。1 GB、2 GB 样例校验 SHA-256，其他样例记录生成参数、大小和哈希。

子进程使用 Windows Job：私有提交最多 6 GiB，保留 1.5 GiB 物理内存和 2 GiB 提交余量，实际配额还受启动时空闲内存约束。达到配额或超时会终止测试子进程；不应绕过保护运行超大声明夹具。容量阶段受限不能解释为程序本身的硬上限。

## 结果解释

`results.jsonl` 每行对应一次受保护运行，包含测试状态、逐文件结果、内存及保护程序记录。进程返回 0 只表示测试驱动完成，应检查每条记录：

- `pass`：符合预期，包括应拒绝的输入被正常拒绝。
- `fail`：断言失败、子进程异常或超时。
- `resource_limited`：保护程序拦截内存分配或系统余量不足。应结合场景判断：大文件容量探测属于环境限制；很小的损坏文件触发超限分配属于需要处理的异常输入问题。
- 清单中 `expect: observe`：记录未确定的支持策略，例如长路径、扩展名与内容不一致。驱动返回 `pass` 不代表已支持该输入。

`fixtures/oversized-declared-count.vtp` 是 535 字节的最小复现：声明 2,147,483,648 个节点，但只包含三个节点数据。当前版本在 6 GiB 和 256 MiB 两种配额下均触发超限分配并被保护程序终止；没有在无保护条件下验证最终行为。

原始验证记录见 [2026-11-21 报告](../../docs/validation/import-stress-20261121/README.md)。测试代码从当次本机探针整理而来，性能数据对应报告中的源代码提交，不应当作后续版本的测试结果。
