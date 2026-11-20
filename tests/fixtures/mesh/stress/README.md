# 边界网格压测样例

两个 ZIP 各包含一个完整的 VTK PolyData（`.vtp`）三角形边界网格。切换到 `features/preprocess-mesh-processing` 分支，在已安装 Git LFS 的工作副本中运行以下命令，随后解压所需的 ZIP，在工作台的「前处理 → 网格管理 → 导入」中选择解压出的 `.vtp` 文件。

```console
git lfs pull --include="tests/fixtures/mesh/stress/*.vtp.zip"
```

| 解压后的文件 | 字节数 | 节点数 | 三角形数 | 解压后文件的 SHA-256 |
| --- | ---: | ---: | ---: | --- |
| `boundary-1GB.vtp` | 1,000,000,075 | 5,681,813 | 11,363,622 | `5440fb6437ccb04b502f3409d7f52003f50fb0d72ab7821b6c49767545e1b15a` |
| `boundary-2GB.vtp` | 2,000,000,046 | 11,363,631 | 22,727,258 | `0df1243998bd4f09e2b7d1b4014e7fd84a97cd37ce03ec6dfffca3ec67295cf8` |

在 PowerShell 中可用 `Get-FileHash .\boundary-2GB.vtp -Algorithm SHA256` 核对解压后的文件。
