# secondUI NX 插件开发调试日志

**项目名称**：secondUI —— 点到点移动与复制插件  
**开发环境**：NX 2306 / Visual Studio 2022 (v143) / C++17 
---

## 1. SelectObject 命名空间冲突

**现象**：
```
error C2872: "SelectObject": 不明确的符号
可能是 "NXOpen::SelectObject"
或     "NXOpen::BlockStyler::SelectObject"
```

**原因**：
代码中同时使用了 `using namespace NXOpen;` 和 `using namespace NXOpen::BlockStyler;`。  
`NXOpen::SelectObject`（通用 NX 对象选择类）与 `NXOpen::BlockStyler::SelectObject`（对话框选择控件）名称完全相同，编译器无法区分。

**解决方案**：
在函数参数中使用完整限定名：
```cpp
static Body *GetBody(NXOpen::BlockStyler::SelectObject *sel)
```
调用处不需要修改，因为成员变量本身就是 `BlockStyler` 命名空间下的类型。

---

## 2. UFUN C API 在 NX 2306 中被移除

**现象**：
多个 UF 函数报"找不到标识符"：
- `UF_MODL_ask_bounding_box`
- `UF_MODL_transform_entities`
- `UF_UI_select_point`
- `uf5943` / `uf5947`

**原因**：
NX 2306 已淘汰旧版 UF C API，这些函数不再存在于头文件和库中。

**解决方案**：
使用纯 NXOpen C++ API 替代：

| 原 UFUN 函数 | 替代方案 |
|---|---|
| `UF_MODL_ask_bounding_box`（包围盒） | 无直接替代，跳过位移计算 |
| `UF_MODL_transform_entities`（移动） | `BlockFeatureBuilder 新建` + `AddToDeleteList 删除旧体` |
| `uf5943` / `uf5947`（复制） | `BlockFeatureBuilder` 新建 |
| `UF_UI_select_point`（选点） | `point01->Point()` 从控件读坐标 |

---

## 3. Block Styler 对话框撤销 update_cb 中的建模更改

**现象**：
1. 点 Move 按钮 → 立方体成功移动
2. 关闭对话框 → 立方体**回到原始位置**

反复测试确认：操作确实执行了，但对话框一关闭就被撤销。

**诊断过程（尝试了多种方案）**：

| 尝试的方案 | 结果 |
|---|---|
| 在 `update_cb` 中用 `DoUpdate(mark)` 提交 | ❌ 仍被撤销 |
| 使用可见 undo mark（`MarkVisibilityVisible`） | ❌ 仍被撤销 |
| 使用不可见 undo mark（`MarkVisibilityInvisible`） | ❌ 仍被撤销 |
| 在 `cancel_cb` 中用 `UndoToMark` 手动管理 | ❌ 无法对抗 NX 自动撤销 |
| 在 `apply_cb` 中执行（按钮不执行） | ✅ 不被撤销，但用户体验差 |

**根因**：
NX Block Styler 对话框的生命周期会自动撤销 **所有在 `update_cb` 中做的建模更改**。  
这是 NX 内部机制，没有 API 可以绕过。

**最终方案**：
将操作分为**显示操作**和**建模操作**两类：
- `update_cb`（按钮）：只做**显示操作**（`DisplayableObject::Blank()`），NX 不会撤销显示更改
- `apply_cb`（Apply 按钮）：做**建模操作**（`AddToDeleteList` + `DoUpdate`），`apply_cb` 的更改是永久的
- `cancel_cb`（Cancel 按钮）：`Unblank()` 还原显示 + `UndoToMark()` 撤销建模

```
按钮（update_cb）:     Blank(旧体) + 创建新块 + DoUpdate
Apply（apply_cb）:     AddToDeleteList(旧体) + DoUpdate + 更新 undo mark
Cancel（cancel_cb）:   Unblank(旧体) + UndoToMark → 还原到初始状态
OK（ok_cb）:           关闭（不执行操作）
```

---

## 4. Apply 按钮导致操作重复执行

**现象**：
1. 点 Copy 按钮 → 新立方体出现 ✅
2. 点 Apply 按钮 → **又出现一个**立方体 ❌

**原因**：
`update_cb`（按钮点击）中调用了 `apply_cb()` 执行操作。  
Apply 按钮本身也会触发 `apply_cb()`。  
两个入口各执行一次，导致操作重复。

**解决方案**：
分离"执行"与"提交"：
- `update_cb`（按钮）：执行操作（隐藏旧体 + 创建新块）
- `apply_cb`（Apply 按钮）：提交操作（删除旧体 + 更新 undo mark，**不重新执行**）
- 用一个布尔标志 `s_done` / `s_applied` 控制 Apply 不再重复执行

---

## 5. UndoMarkId 枚举初始化

**现象**：
```
error C2440: "初始化": 无法从"int"转换为"NXOpen::Session::UndoMarkId"
```

**原因**：
`Session::UndoMarkId` 是 C++ 枚举类型，不能直接用整数字面量 `0` 初始化。

**解决方案**：
```cpp
// 错误
static Session::UndoMarkId s_mark = 0;

// 正确
static Session::UndoMarkId s_mark = (Session::UndoMarkId)0;
```
C 风格强制转换可将整数值转为枚举值。

---

## 6. DllExport 宏未定义

**现象**：
```
error C2079: "secondUI" 使用了未定义的 class "DllExport"
```

**原因**：
`DllExport` 宏定义在 `uf_defs.h` 中。清理头文件时误删了该 include。  
之后的所有类定义都因为 `class DllExport secondUI` 中的 `DllExport` 未定义而报错。

**解决方案**：
恢复 `#include <uf_defs.h>`。  
该头文件只包含 `DllExport`、`tag_t` 等类型和宏定义，不包含任何 UF 函数声明，保留它不算违反"禁止使用 UFUN"的要求。

---


---

## 7. FindObject 在特征未找到时抛出异常

**现象**：
尝试用 `Features()->FindObject("BLOCK(0)")` 遍历特征时，  
第一个不存在的索引就导致程序崩溃（抛出 NXException）。

**原因**：
`FeatureCollection::FindObject()` 在找不到对象时抛出 `NXException`，而不是返回 `NULL`。  
循环中未处理该异常。

**解决方案**：
在调用处添加 try-catch：
```cpp
for (int i = 0; i < 1000; i++) {
    char n[64]; snprintf(n, 64, "BLOCK(%d)", i);
    Features::Feature *f = NULL;
    try { f = wp->Features()->FindObject(n); }
    catch (NXOpen::NXException&) { break; }  // 没找到就退出循环
    if (!f) break;
    // ... 处理 f
}
```

---

## 8. 选中体被删除后 Apply 按钮失效

**现象**：
Move 操作后旧体被 `AddToDeleteList` 标记删除，`selection0` 的选中状态变为无效。
Apply 按钮被对话框禁用，无法点击。

**解决方案**：
Move 按钮不删除旧体，只隐藏（`Blank()`）。  
Apply 时才执行 `AddToDeleteList(body)` + `DoUpdate()` 真正删除。  
这样在整个操作过程中 `selection0` 的选中状态始终有效。

---

## 关键 API 可用性一览

| API | 存在 | 备注 |
|---|---|---|
| `DisplayableObject::Blank()` | ✅ | 隐藏体（显示操作） |
| `DisplayableObject::Unblank()` | ✅ | 显示体 |
| `UpdateManager::AddToDeleteList()` | ✅ | 标记删除 |
| `UpdateManager::DoUpdate(mark)` | ✅ | 处理更新 |
| `Session::SetUndoMark()` | ✅ | 创建 undo mark |
| `Session::UndoToMark()` | ✅ | 撤销到指定 mark |
| `BlockFeatureBuilder` | ✅ | 创建方块特征 |
| `SpecifyPoint::Point()` | ✅ | 读取点坐标 |
| `SelectObject::GetSelectedObjects()` | ✅ | 获取选中对象 |
| `Body::GetBoundingBox()` | ❌ | 无替代 |
| `Body::GetFeature()` | ❌ | 无替代 |
| `FeatureCollection::begin()/end()` | ❌ | 迭代器不可用 |
| `MoveObjectBuilder` | ❌ | NX 2306 不存在 |
| `Part::BaseFeatures()` | ❌ | NX 2306 不存在 |
| `UF_MODL_ask_bounding_box` | ❌ | 已移除 |
| `UF_MODL_transform_entities` | ❌ | 已移除 |
| `uf5943` / `uf5947` | ❌ | 已移除 |

---

## 最终代码流程总结

```
对话框打开 → dialogShown_cb → s_mark = SetUndoMark(Invisible)
                                s_applied = false

点 Move 按钮 → body->Blank()              ← 隐藏旧体
               CreateBlockFeatureBuilder ← 创建新体
               DoUpdate(s_mark)
               s_applied = false

点 Copy 按钮 → CreateBlockFeatureBuilder  ← 直接创建
               DoUpdate(s_mark)
               s_applied = false

点 Apply 按钮 → AddToDeleteList(body)     ← 仅 Move 时删除
                DoUpdate(s_mark)
                s_mark = SetUndoMark(...) ← 提交
                s_applied = true

点 Cancel 按钮 → if !s_applied:
                   body->Unblank()       ← 还原显示
                   UndoToMark(s_mark)    ← 撤销建模

点 OK 按钮 → 关闭对话框（不做任何操作）
```
