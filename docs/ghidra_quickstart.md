# Ghidra Quickstart — 解 CDBTableDataStorage 的 wire format

针对 Silent Storm port 的具体目标：把 `MapEdit.exe` 的 5 个 wire-handler 反编译出来，让 r11 的空 stub 升级成真正能填 records 的实现。

## 启动

1. 双击桌面快捷方式 `Ghidra` （或运行 `C:\Tools\ghidra_12.0.4_PUBLIC\ghidraRun.bat`）。第一次启动会让你同意许可协议。
2. **新建 Project**：`File → New Project → Non-Shared Project`。命名 `silent-storm-re`，存到 `C:\Users\Haohan\Documents\silent-storm\re\`。
3. **导入 MapEdit.exe**：`File → Import File`，选 `C:\Program Files (x86)\Steam\steamapps\common\Silent Storm\MapEdit.exe`。
   - Format: Portable Executable (PE)
   - Language: `x86:LE:32:default` — **32-bit，重要**（Silent Storm 是 x86）
   - 点 OK，再 OK 让它导入。
4. **双击 MapEdit** 打开 CodeBrowser。弹"Analyze?"对话框，点 **Yes**，全部默认勾选，OK。
   - 第一次分析约 3-10 分钟，看进度条左下角。
   - 期间可以喝杯咖啡。

## 跳到目标函数

分析跑完，按 **`G`** 键（Go to address），输入：

| 地址 | 是什么 |
|---|---|
| `0044CA30` | `CDBTableDataStorage::operator&(CStructureSaver&)` — 主 Serialize |
| `0044CB30` | sub-handler 1 — std::vector\<12-byte\> 主桶 |
| `0044CC90` | sub-handler 2 |
| `0044D810` | sub-handler 3 |
| `0044D360` | sub-handler 4 |
| `0044CEC0` | sub-handler 5（被 fields 6/7/8 共用） |

跳过去看到反汇编后，按 **`F`** 让 Ghidra 把这块标成 function。然后按 **`Ctrl+L`** 重命名（比如把 `FUN_0044ca30` 改成 `CDBTableDataStorage__op_amp`）。

## 看反编译（这是关键）

光标停在反汇编里任何一行，按 **`Ctrl+E`** 打开 **Decompiler** 窗口，或 `Window → Decompiler`。右边会显示 C 风格的反编译伪代码。

Ghidra 的反编译器很强，会自动还原 `if/while/for`、变量、参数。对 `0044CA30` 看到的应该类似：

```c
int FUN_0044ca30(int *this, int *saver) {
  if (CStructureSaver__IsField(saver, 2, 1)) {
    FUN_0044cb30(saver, (char *)this + 0x0c);
    CStructureSaver__FinishField(saver);
  }
  if (CStructureSaver__IsField(saver, 3, 1)) { ... }
  // ... 8 fields total
}
```

把变量重命名（光标停在 `local_4` 上按 **`L`**）能让代码越来越像源码。

## 关键技巧

### 1. 跨引用（XRef）
- 选中一个函数名 / 地址，按 **`Ctrl+Shift+F`** 看谁调用它。
- 我们已经知道 `0x0044B310` 是 `CDBTableDataStorage` 的 factory。看它被谁调用 → 找到 `RegisterClass(0xA1843130, ...)` 的位置。

### 2. 结构体定义
- `Data Type Manager` 窗口（默认右下）右键 → `New → Structure`，定义一个 `CDBTableDataStorage` 结构体（136 字节，已知字段偏移）。
- 然后在反编译窗口右键变量 → `Auto Create Structure` 或 `Set Data Type` → 选你定义的结构。立刻所有 `*(int*)((char*)this+0x0c)` 都会变成 `this->m_buckets_begin`。

### 3. 字符串
- `Window → Defined Strings`。搜 `CDBTableDataStorage`、`Records`、`Hash`、`Storage` 等关键词。MapEdit 里的字符串经常能给你函数名或类名提示。

### 4. RTTI 自动恢复
- `Window → Script Manager` → 搜 `RecoverClassesFromRTTIScript`，双击运行。它会扫 `.data` 里所有 RTTI 描述符，自动给类创建结构体和方法。
- 跑完后 `CDBTableDataStorage` 应该会被自动识别。

## 我们要恢复的具体信息

每个 sub-handler（5 个），需要搞清楚两件事：

1. **wire 字段顺序**：handler 内部按什么顺序读什么类型的字段（`int`，`std::string`，`CObj<T>` 指针，嵌套结构...）
2. **目标 12 字节结构**：handler 写入的 12 字节是哪三个 dword（key, value-ptr, hash？还是 vec3?）

把每个 handler 看一遍，写成 C++ 等价代码贴到 `port/docs/patches/p1_5_r12_wire_handlers.md`。然后给我看，我把 stub 升级成真实现。

## 卡住时

- **看不懂某条汇编指令**：`Help → Contents → User Manual → Code Browser` 或直接 Google "Ghidra x86 mnemonic"
- **反编译器输出乱**：可能因为参数推断错了。光标在函数签名上按 **`F`** 重新定义 function；或菜单 `Function → Edit Function Signature` 手动改 calling convention（`__thiscall` 对应 C++ 成员函数，`__cdecl` 对应自由函数）。
- **想保存进度**：`File → Save` 经常按。Project 会自动保存分析结果到 `re\` 目录。
- **想问我**：截图贴回来或者把反编译器输出复制粘贴过来。

## 不要做的事

- 不要修改 MapEdit.exe 字节（我们只读）
- 不要把整个 .text 段全 export — 太大没用
- 不要用 Ghidra Server / collaboration 功能 — 单机够了
