# 本地实验资料

此目录是本分支新增的课程资料，不属于上游 SDK 的文档清理范围。
`PSoC开发实践指南.htm`、其 `.files/` 中的 HTML/XML，以及
`RT-Thread-ENV使用说明.md` 作为文本保留在 Git。

## 不能由 Env 生成的资源

| 文件 | 获取方式与用途 |
| --- | --- |
| `2026研究生课程项目列表.docx` | 向课程资料提供者获取原始文件；也可使用下述本仓库固定历史快照 |
| `PSoC开发实践指南.files/` 中的 24 张 PNG/JPG 图片 | 使用同一份指南 HTML 导出包中的 `.files/` 目录；具体清单见 `PSoC开发实践指南.files/filelist.xml` |
| `PSoC开发实践指南.files/themedata.thmx` | 同一份 Word HTML 导出包的主题文件 |

这些文件不能从保留的 HTML/XML 反向复原，也不是 `pkgs --update` 的产物。
它们已加入本目录的忽略规则，停止跟踪时保留本地副本。纯文本可直接阅读，
但完整显示指南插图仍需恢复对应资源。

本仓库已有固定快照 `cba777aacd5b9a3650259113019bd0fb8a0f1710`
（`origin/experiment4` 的历史）。完整克隆中可从历史恢复原始字节，无需脚本；
在 **SDK 根目录**执行以下命令，只恢复二进制，不覆盖 HTML/XML/Markdown：

```bat
git restore --source=cba777aacd5b9a3650259113019bd0fb8a0f1710 --worktree -- "projects/exp-docs/2026研究生课程项目列表.docx"
git restore --source=cba777aacd5b9a3650259113019bd0fb8a0f1710 --worktree -- ":(glob)projects/exp-docs/PSoC开发实践指南.files/*.png" ":(glob)projects/exp-docs/PSoC开发实践指南.files/*.jpg" "projects/exp-docs/PSoC开发实践指南.files/themedata.thmx"
```

上述命令会替换同名本地文件，已有自行修改的副本应先保存到其他位置。
`--worktree` 不重新加入 Git 索引。浅克隆缺少该快照时，先在本 fork 获取历史：

```bat
git fetch https://github.com/goldfish9901/sdk-bsp-psoc_e84-edgi-talk.git cba777aacd5b9a3650259113019bd0fb8a0f1710
```

上游 RT-Thread 仓库没有这些课程资料，不能用上游 SDK 下载包代替。
本次只停止新版本跟踪，不清除旧 Git 历史中的资源。
