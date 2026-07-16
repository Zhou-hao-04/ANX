# dianCopyMove 插件开发调试日志

> 项目: dianCopyMove — NX 2306 Open C++ 点选复制/移动插件  
> 开发日期: 2026-07-15  
> 开发环境: Visual Studio 2022, Siemens NX 2306  
> 目标文件: `diancm.hpp` / `diancm.cpp`  

---

## 目录

1. [项目概况](#1-项目概况)
2. [问题一：链接错误 LNK2019 — 缺少 NXOpen C++ 库](#2-问题一链接错误-lnk2019--缺少-nxopen-c-库)
3. [问题二：Windows 宏命名冲突](#3-问题二windows-宏命名冲突)
4. [问题三：apply_patch 残留标记](#4-问题三apply_patch-残留标记)
5. [问题四：vcxproj PropertyGroup 闭合断裂](#5-问题四vcxproj-propertygroup-闭合断裂)
6. [问题五：输出目录配置](#6-问题五输出目录配置)
7. [问题六：多重 try 语句](#7-问题六多重-try-语句)
8. [问题七：Invalid property name "MaximumSelection"](#8-问题七invalid-property-name-maximumselection)
9. [问题八：m_startPtClicked 永远为 false](#9-问题八m_startptclicked-永远为-false)
10. [问题九：update_cb if-else 链断裂](#10-问题九update_cb-if-else-链断裂)
11. [问题十：多选时起点/终点逻辑优化](#11-问题十多选时起点终点逻辑优化)
12. [附录：当前链接库配置](#12-附录当前链接库配置)

---

## 1. 项目概况

### 1.1 功能目标

实现 Block Styler 对话框驱动的点选复制/移动功能：

- **dotAction**: 点选 + 旋转移动/复制（参考实现，使用 `MoveObjectBuilder`）
- **Hiabr_DotMove_NX2306**: 点到点平移移动/复制（参考实现，使用 `UF_MODL_transform_entities` / `UF_TRNS`）
- **diancm**: 主目标文件，Block Styler 框架实现的点选复制/移动对话框

### 1.2 对话框 UI 结构

```
group0 (工具栏)
  ├─ button0  → 移动
  └─ button01 → 复制
group (对象选择)
  └─ selection0 → SelectObject
group2 (起点)
  ├─ point0 → SpecifyPoint (手动点)
  └─ enum0  → Enumeration (起点模式)
group3 (终点)
  ├─ point01 → SpecifyPoint (手动点)
  └─ enum01  → Enumeration (终点模式)
```

### 1.3 代码架构

```
┌──────────────────────────────────────────────┐
│  anonymous namespace (工具函数层)              │
│  ├─ StartPortMode / FinishPointMode 枚举      │
│  ├─ DLX 路径解析 (GetDllDirectory / ResolveDlxPath) 
│  ├─ 几何工具 (AskBoxCenter / AskBodyPoint)    │
│  ├─ Tag 解析 (ResolveBodyTag)                 │
│  ├─ 选择工具 (CollectTransformTags / PrimarySelection)
│  ├─ 点解析 (ResolveStartPoint / ResolveFinishPoint)
│  ├─ 变换执行 (ExecuteMoveCopy)                │
│  └─ SetPointOptional (标记点块为可选)          │
├──────────────────────────────────────────────┤
│  diancm 类 (Block Styler 回调层)               │
│  ├─ initialize_cb() — 绑定 UI 块              │
│  ├─ update_cb() — 处理交互事件                │
│  ├─ apply_cb() / ok_cb() — 核心执行逻辑       │
│  ├─ UpdateStartPointUI() — 多选/单选 UI 切换   │
│  └─ 辅助方法: GetSelectedObjects / GetEnumValue
└──────────────────────────────────────────────┘
```

---

## 2. 问题一：链接错误 LNK2019 — 缺少 NXOpen C++ 库

### 症状

生成解决方案时报约 30 个 LNK2019：

```text
error LNK2019: 无法解析的外部符号 "public: static class NXOpen::Session * ...
error LNK2019: 无法解析的外部符号 "public: static class NXOpen::UI * ...
```

以及后续的 LNK1120:

```text
error LNK1120: 30 个无法解析的外部命令
```

### 根因分析

项目最初由 NX12 Open AppWizard 生成（`dianCopyMove.c` 纯 C 入口），`vcxproj` 的 `<AdditionalDependencies>` 只配置了 C 层 UF 库：

```xml
<AdditionalDependencies>
    libufun.lib;libufun_cae.lib;...    ← 只有 C 层 UF 库
</AdditionalDependencies>
```

但 `diancm.cpp` 大量使用 NXOpen C++ 类（`Session`、`UI`、`Part`、`Body`、`BlockStyler::*` 等），这些类的实现在 `libnxopencpp.lib` 中，链接器找不到。

### 解决方案

在 `Debug|x64` 和 `Release|x64` 两个配置的 `<AdditionalDependencies>` 中追加 `libnxopencpp.lib`：

```xml
<AdditionalDependencies>
    libnxopencpp.lib;libufun.lib;...    ← 新增
</AdditionalDependencies>
```

### 后续补充：BlockStyler 符号仍需另一个库

添加 `libnxopencpp.lib` 后，仍有 ~17 个 LNK2019（`SpecifyPoint::Point()` 等 BlockStyler 符号）。

尝试 `libnxopencpp_blocks.lib` → **LNK1104 文件未找到**（此库在 NX 2306 中已不存在）。

检查 NX 2306 安装目录 `D:\Program Files\UG_NX\ugopen\` → 发现 `libnxopenuicpp.lib`。

替换为 `libnxopenuicpp.lib` → 链接成功。

**最终链接库清单**：

```xml
libnxopencpp.lib;libnxopenuicpp.lib;libufun.lib;libufun_cae.lib;libufun_cam.lib;
libufun_die.lib;libufun_vdac.lib;libufun_weld.lib;libugopenint.lib;
libugopenint_cae.lib;libugopenint_cam.lib
```

---

## 3. 问题二：Windows 宏命名冲突

### 症状

```text
error C2868: "SelectObject": 不明确的符号
error C2868: "Group": 不明确的符号
```

发生在 `initialize_cb()` 中的 `dynamic_cast` 行。

### 根因分析

两个冲突来源：

| 符号 | 冲突源 | 说明 |
|------|--------|------|
| `SelectObject` | `windows.h` GDI 宏 | 和 `CreateDialog` 同样的问题 |
| `Group` | MFC (`UseOfMfc>Dynamic`) | 项目启用了 MFC，引入同名类型 |
| | `windows.h` 某些版本 | 部分 WinSDK 版本也定义 `Group` |

原模板使用全限定名 `NXOpen::BlockStyler::Group*`，但我重写的 `initialize_cb` 中使用了未限定的 `dynamic_cast<Group*>(...)`。

### 解决方案

两处修复：

**① 添加 `#undef SelectObject`**（类似 `#undef CreateDialog`）：

```cpp
#include <windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif
#ifdef SelectObject
#undef SelectObject
#endif
```

**② 全部 `dynamic_cast` 使用全限定名**：

```cpp
// 之前（编译不过）
group0 = dynamic_cast<Group*>(...);
selection0 = dynamic_cast<SelectObject*>(...);

// 之后（OK）
group0 = dynamic_cast<NXOpen::BlockStyler::Group*>(...);
selection0 = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(...);
```

其他 BlockStyler 类型（`Button*`、`SpecifyPoint*`、`Enumeration*`）也一并全限定，保持一致性。

---

## 4. 问题三：apply_patch 残留标记

### 症状

```text
error C2146: 语法错误: 缺少";"(在标识符"End"的前面)
error C2143: 语法错误: 缺少";"(在"*"的前面)
```

### 根因分析

使用 `apply_patch` 工具的 `*** Add File` 语法创建文件时，工具自动在文件末尾写入 `*** End of File` 标记行。此标记不是合法 C++ 代码。

### 解决方案

使用 PowerShell 删除含有 `*** End of File` 的行：

```powershell
$content = $content | Where-Object { $_ -notmatch '\*\*\* End of File' }
```

---

## 5. 问题四：vcxproj PropertyGroup 闭合断裂

### 症状

```text
MSBuild 错误: 意外的 XML 元素 "</Project>"
```

### 根因分析

修改 `vcxproj` 中的 PropertyGroup 时，替换逻辑覆盖了 `</PropertyGroup>` 闭合标签：

```
原结构:                   替换后（错误）:
<PropertyGroup ...>       <PropertyGroup ...>
    <LinkIncremental>         <OutDir>...</OutDir>
</PropertyGroup>              <LinkIncremental>
                                  ← 缺少 </PropertyGroup>
```

后续的 XML 解析全部断裂。

### 解决方案

采用行级操作，显式保留闭合标签：

```powershell
$lines[$i+1] = "    <OutDir>D:\A NX FILE\Application\</OutDir>"
$lines[$i+2] = "    <LinkIncremental>true</LinkIncremental>"
# $lines[$i+3] 原本就是 </PropertyGroup>，保持不变
```

---

## 6. 问题五：输出目录配置

### 需求

将编译输出的 `.dll` 文件直接生成到 `D:\A NX FILE\Application\`。

### 解决方案

在 `Debug|x64` 和 `Release|x64` 的 PropertyGroup 中添加 `<OutDir>`：

```xml
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <OutDir>D:\A NX FILE\Application\</OutDir>       ← 新增
    <LinkIncremental>true</LinkIncremental>
</PropertyGroup>
```

同时清理 `<OutputFile>` 和 `<ImportLibrary>` 中的路径分隔符：

```xml
<!-- 之前 -->
<OutputFile>$(OutDir)/dianCopyMove.dll</OutputFile>
<!-- 之后（依赖 OutDir 尾部反斜杠，移除多余 /） -->
<OutputFile>$(OutDir)dianCopyMove.dll</OutputFile>
```

---

## 7. 问题六：多重 try 语句

### 症状

```text
error C2143: 语法错误: 缺少"{"
```

指向 `update_cb` 函数的第 543 行附近。

### 根因分析

重写 `update_cb` 的 `if-else` 链时，新的链数组第一行是 `'    try'`，但被替换的原始代码中已经包含 `try`，导致双写：

```cpp
int diancm::update_cb(...)
{
    try
    try           ← 多出来的一个！
    {
        if (...) ...
```

### 解决方案

查找相邻的两个 `try`，删除多余的：

```powershell
if ($lines[$i].Trim() -eq 'try' -and $lines[$i+1].Trim() -eq 'try') {
    $lines = $lines[0..($i-1)] + $lines[($i+1)..($lines.Count-1)]
}
```

---

## 8. 问题七：Invalid property name "MaximumSelection"

### 症状

对话框打开时 NX 弹出错误框：

> Invalid property name for the block. See syslog for details.

### 根因分析

在 `initialize_cb` 中尝试设置选择上限：

```cpp
PropertyList* selProps = selection0->GetProperties();
selProps->SetInteger("MaximumSelection", 0);    ← 属性名错误
```

Syslog 确认（`%TEMP%\3264583cf76c.syslog`）：

```
*** EXCEPTION: Error code 3520041
+++ Property "MaximumSelection" does not exist for block name "selection0"
```

在 NX 2306 中，`SelectObject` 块没有叫 `"MaximumSelection"` 的属性。

### 解决方案

删除代码中的属性设置。由用户在 Block Styler 中配置：

1. **Tools → Block Styler** → 打开 `diancm.dlx`
2. 选中 `selection0`
3. 属性面板 → **SelectMode** → 从 `Single` 改为 `Multiple`

并在代码的 `UpdateStartPointUI()` 中尝试运行时切换：

```cpp
try {
    PropertyList* props = selection0->GetProperties();
    props->SetEnum("SelectMode", multiEnabled ? 1 : 0);
} catch (...) { }
```

---

## 9. 问题八：m_startPtClicked 永远为 false

### 症状

多选实体后，在面上点选指定起点，设好终点，点击应用：

> 多选模式下请先在实体面上指定起始点

### 根因分析

`update_cb` 中 `point0` 的 handler 写错了变量：

```cpp
// 错误
else if (block == point0)
{
    m_startPtSpecified = true;    ← 应该是 m_startPtClicked！
}

// apply_cb 中检查
else if (m_startPtSpecified && point0 && !m_startPtClicked)
                    ↑ true                ↑ 永远 false → 走进错误分支
```

根因是之前重写 `update_cb` 的 `if-else` 链时，我错误地复用了 `m_startPtSpecified`。

此外，`SpecifyPoint` 块触发 `update_cb` 的行为在不同 NX 版本中不可靠——可能根本不触发。所以 `m_startPtClicked` 的追踪机制本身就有缺陷。

### 解决方案

两处修复：

**① 修正 update_cb 的变量名**：

```cpp
else if (block == point0)
{
    m_startPtClicked = true;     ← 修正
}
```

**② 简化 apply_cb 的起点逻辑**，移除对 `m_startPtClicked` 的强依赖：

```cpp
// 之前（三个分支，依赖 m_startPtClicked）
if (m_startPtSpecified && point0 && m_startPtClicked)  { ... }
else if (m_startPtSpecified && point0 && !m_startPtClicked)  { throw; }
else { ... }  // 枚举模式

// 之后（两个分支，不依赖 m_startPtClicked）
if (m_startPtSpecified && point0)
{
    start = point0->Point();     // 直接读取点值
}
else
{
    start = ResolveStartPoint(enum0模式, primary, point0);
}
```

---

## 10. 问题九：update_cb if-else 链断裂

### 症状

```text
error C2146: 语法错误
```

### 根因分析

向 `update_cb` 的 `else if (block == enum01)` 后插入点追踪代码时，插入位置错误地把原 `{ ... }` 体和 `else if` 分开了：

```cpp
// 意图
else if (block == enum01)
{
    // end point mode changed
}
else if (block == point0) { m_startPtSpecified = true; }

// 实际
else if (block == enum01)
else if (block == point0) { m_startPtSpecified = true; }
    // ← 原 body 变成了孤立的 { }
{
    // end point mode changed
}
```

产生了三个问题：
1. `else if (block == enum01)` 无 body（语法断裂）
2. 孤立的 `{ // end point mode changed }` 块（游离代码）
3. 后续所有 `m_startPtSpecified` / `m_endPtSpecified` 引用都因为语法错误而编译不过

### 解决方案

直接完整重写 `update_cb` 的整个 `if-else` 链，确保所有 `else if` 都有正确的 `{ }` 体：

```cpp
if (block == button0)  { m_opMode = OpMode::Move; }
else if (block == button01)   { m_opMode = OpMode::Copy; }
else if (block == selection0) { UpdateStartPointUI(); }
else if (block == enum0)      { /* start point mode changed */ }
else if (block == enum01)     { /* end point mode changed */ }
else if (block == point0)     { m_startPtClicked = true; }
else if (block == point01)    { m_endPtSpecified = true; }
else if (block == toggle0)    { UpdateStartPointUI(); }    ← 复选框
```

---

## 11. 问题十：多选时起点/终点逻辑优化

### 功能需求

- **单选模式**（默认）：用 `enum0` 选择起点模式（体中心/体中心上等），用 `enum01` / `point01` 设终点
- **多选模式**（勾选 `toggle0`）：
  - `enum0` 自动禁用（灰色）
  - 用户必须在 `point0` 中点选一个面上的点作为起点
  - 所有实体以 起点→终点 向量整体平移

### 实现方案

在 `UpdateStartPointUI()` 中动态切换：

```cpp
void diancm::UpdateStartPointUI()
{
    if (!selection0 || !enum0 || !point0 || !toggle0) return;

    bool multiEnabled = toggle0->Value();

    // 运行时切换 SelectMode (Single/Multiple)
    try {
        PropertyList* props = selection0->GetProperties();
        props->SetEnum("SelectMode", multiEnabled ? 1 : 0);
    } catch (...) { }

    if (multiEnabled)
    {
        int count = selection0->GetSelectedObjects().size();
        if (count >= 2)
        {
            enum0->SetEnable(false);
            m_startPtSpecified = true;
        }
        else
        {
            enum0->SetEnable(true);
        }
    }
    else
    {
        enum0->SetEnable(true);
        m_startPtSpecified = false;
        m_startPtClicked = false;
    }
}
```

### 用户操作流程

```
勾选"多选模式" → SelectMode = Multiple
              → 选中 ≥2 个实体 → enum0 禁用
              → point0 点选面上的起点
              → 设终点 → 应用

取消"多选模式" → SelectMode = Single
              → enum0 启用
              → 普通单选流程
```

---

## 12. 附录：当前链接库配置

### Debug|x64

```xml
<AdditionalDependencies>
    libnxopencpp.lib;libnxopenuicpp.lib;
    libufun.lib;libufun_cae.lib;libufun_cam.lib;
    libufun_die.lib;libufun_vdac.lib;libufun_weld.lib;
    libugopenint.lib;libugopenint_cae.lib;libugopenint_cam.lib;
    %(AdditionalDependencies)
</AdditionalDependencies>
<AdditionalLibraryDirectories>
    $(UGII_BASE_DIR)\ugopen;%(AdditionalLibraryDirectories)
</AdditionalLibraryDirectories>
```

### Release|x64

同上。

### 输出目录

```xml
<OutDir>D:\A NX FILE\Application\</OutDir>
```

---



## 13. 代码优化：消除冗余与提升可读性

> **日期**: 2026-07-16
> **目标文件**: diancm.hpp / diancm.cpp
> **初始行数**: 638 行（.cpp）
> **优化后行数**: 648 行（.cpp，净减少 30 行死代码，消除 28 行装饰分隔线）

### 优化背景

在代码审查中发现 diancm.cpp 存在以下问题：
- **死变量**: m_startPtClicked -- 有写入无读取
- **死函数**: GetSelectedObjects() -- 定义后无一调用点
- **冗余异常处理**: ok_cb() 的外层 try-catch 完全被 apply_cb() 内部的异常处理覆盖
- **过度装饰**: 31 行分隔线（占全文件 5%）
- **隐晦生存期**: static std::string + c_str() 无注释说明
- **空骨架**: ufusr_cleanup() 的空 try-catch 徒增阅读负担

### 改动汇总（6 项）

| # | 改动 | 涉及文件 | 行数影响 | 功能影响 | 风险 |
|---|---|---|---|---|---|
| 1 | 删除 m_startPtClicked 变量 | .hpp + .cpp（4 处） | -4 行 | 无（死赋值） | 低 |
| 2 | 删除 GetSelectedObjects() 函数 | .hpp + .cpp | -14 行 | 无（死函数） | 低 |
| 3 | 简化 ok_cb() 消除冗余 try-catch | .cpp | -15 行 | 极低 | 低 |
| 4 | 压缩过度分隔线 | .cpp（全文件） | -28 行 | 无（纯格式） | 无 |
| 5 | 为 static std::string 添加生存期注释 | .cpp | +1 行 | 无（纯注释） | 无 |
| 6 | 消减 ufusr_cleanup() 空 try-catch 骨架 | .cpp | -8 行 | 极低 | 低 |

### 与历史修复的交互验证

逐条比对了 DEBUG_LOG 第 2-11 节记录的所有已解决问题，确认本次优化不会与任何历史修复产生冲突：

- #6 多重 try 语句：当时修复的是 update_cb 内的双 try，本次改动的是 ok_cb 的外层 try，不同位置
- #8 m_startPtClicked 永远为 false：当时修正了变量名错用，但此后已沦为死赋值。本次删除它是逻辑延续，不是回退
- #9 update_cb if-else 链断裂：链结构完整，本次只将赋值替换为注释
- #10 多选时起点/终点逻辑优化：m_startPtSpecified 在多选 >= 2 时的赋值逻辑完整保留
- 其余问题(#1-#7)：无交集

---
