# 图片浏览窗口实现审查记录

日期：2026-11-20。设计待确认，尚未修改产品代码。

## 已确认行为与下一步

| 项目 | 当前源码证据 | 结论与所需验证 |
| --- | --- | --- |
| 转换后编辑状态 | ImageBrowserWindow::save 仅在 format == pdf 时提前返回，其余格式调用 document_.load；convertFormat 调用同一个 save 入口 | 非 PDF 转换建立新文档，清除撤销历史；按设计统一为导出需要修改窗口流程，并补非 PDF 导出保留源图和历史的 UI 回归 |
| 保存结果表示 | ImageDocument::save 将编码前 output 放入 SaveResult::image；窗口把它作为保存后基线，常规保存随后重新加载文件 | 有损编码下内存基线与文件实际像素可能不同；需验证各调用路径，避免用编码前像素表示已保存文件。不能据此断言常规保存最终显示错误 |
| 长编辑链 | 普通新增编辑走 incremental；调色从 colorBase_.current 处理；撤销/重做调用默认 renderDocument，从 candidate.original 重放 | 新增编辑和调色已有优化；撤销/重做长链需测量，不应笼统认定所有操作均反复重放 |
| 删除安全 | deleteImage 使用 QFile::moveToTrash，失败后提示并返回 | 已有安全路径。增加失败状态与动画恢复的覆盖，不引入永久删除回退 |
| 图像方向与色彩 | read/readFrame 调用 setAutoTransform(true)；render 末尾保留 original.colorSpace | 已有方向处理与色彩标签保留。仍需实际 EXIF/ICC 样本证明正确性，不能因缺少统一转换而直接判定色彩错误 |
| 保存冲突与原子写入 | QSaveFile 关闭直接写回退，提交前检查源文件时间和大小 | 现有保护可保留；时间与大小不是内容身份的完整证明，需评估同大小同时间外部替换的覆盖及平台限制 |

## 现有自动测试覆盖

ImageBrowserTest.cpp 已包含 PNG 原子保存和往返、无效格式不破坏文件、目录不存在、提交前冲突拒绝、JPEG/BMP、输出尺寸与灰度、质量和压缩参数、ICO 尺寸、PDF 导出保留历史、调色撤销合并、标注/马赛克/抠图撤销、目录监听不夺取编辑状态、万图扫描及超大图受限预览等。现有覆盖不等于上述新增要求已验证。

基线命令：Visual Studio 内置 ctest.exe --test-dir build/cmake-x64-Release -R ^image_browser_workflow$ --output-on-failure。
结果：1/1 通过，测试耗时 10.10 秒。此次未重新构建，该结果仅对应现有构建产物，不能作为当前源码全量验证或优化完成证据。

## 后续实施入口

先确定统一导出语义，补失败场景回归并实现窗口状态隔离，再测量长链性能。新增缓存或任务调度机制须根据测量决定。设计中包含的其他工具质量、DPI、键盘、动画及中英文回归仍须逐项执行。
