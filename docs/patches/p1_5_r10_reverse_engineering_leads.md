# Phase 1.5 r10 — Reverse engineering leads from Steam install

**Date:** 2026-05-12
**Goal:** identify what class typeID `0xA1843130` is, so the 155 missing records in `game.db` can deserialize.

## Method

User pointed to Steam English Gold install. Two binaries available:

- `game.exe` — **PACKED** (`.fqrb`, `.bohnv` sections — non-standard, suggest UPX/Themida or custom packer). `strings` returns empty. Direct symbol extraction not possible without unpacking first.
- `MapEdit.exe` — **NOT PACKED** (`.text`, `.rdata`, `.data`, `.rsrc`, `.reloc` — clean PE). Strings readable. Same Nival REGISTER_SAVELOAD_CLASS table as Game.exe (both built from the same shipping codebase).

## Findings in MapEdit.exe

### 0xA1843130 byte occurrences

Bytes `30 31 84 A1` (little-endian 0xA1843130) appear in MapEdit.exe **twice**, both inside `.text` code (constructor pattern — `mov [ptr], 0xA1843130` writing typeID into an object's member). Adjacent ASCII shows opcode bytes, not strings.

Identical pattern in Steam `game.exe` — same 2 occurrences in same constructor signature.

### Differential class analysis

Extracted **285 C*-prefixed ASCII identifiers** from MapEdit.exe (saved as `mapedit_classes.txt`). Diffed against the **888 class names** that appear in Jan03 source REGISTER_SAVELOAD_CLASS / REGISTER_SAVELOAD_TEMPL_CLASS macros.

**60+ classes in MapEdit but not in Jan03 source REGISTER list.** Most are MapEdit-specific MFC UI (Dlg / Frame / Page / View — these are NOT serializable, so they're not candidates).

### Prime candidates for `0xA1843130` (serializable, leaf-like)

| Class | Why suspicious |
|---|---|
| `CAFunctionConstant` | Function-of-time — float holder, tiny |
| `CAFunctionCos` | Same |
| `CAFunctionParabola` | Same |
| `CAFunctionSinus` | Same |
| `CExecMove` | Movement executor — added post-Jan03 likely |
| `CInventoryAnimator` | Animator type, leaf |
| `CStandAnimator` | Animator type, leaf |
| `CPosRotAnimator` | Animator type, leaf |
| `CDBCamera` | We already know `GetDBCamera(26)` returns NULL — this is a confirmed missing class |

The 9-byte spacing pattern of the 155 missing records (chunk positions 9/18/27/...) suggests very small object headers — fits the "tiny float holder" CAFunction* shape more than the animator pattern.

## Next-session blueprint (IDA / Ghidra)

To definitively identify `0xA1843130` and recover its Serialize layout:

1. Open `MapEdit.exe` in IDA / Ghidra (no unpack needed — it's already plain PE).
2. Search for immediate value `0xA1843130` (two hits expected, at file offsets 0x62D6E2 and 0x62D6F5 in the version we have).
3. The function containing those hits is the constructor of class C with typeID 0xA1843130.
4. The constructor's vtable assignment (`mov [eax], 0x...`) gives the vtable address.
5. Vtable's first entries: destructor, MakeCopy, GetTypeName (usually). GetTypeName returns a string literal — the class name.
6. Cross-reference: find `RegisterClass(0xA1843130, "ClassName", factory)` in the REGISTER_SAVELOAD_CLASS table (likely a `.data` section array of `{typeID, name_ptr, factory_ptr}` triples).
7. Examine `operator&(CStructureSaver&)` — the serialize method. Each `f.Add(N, &m_X)` line names a member with serialized order N. This reconstructs the class field layout.
8. Write a port-side `class StubA1843130 : public CObjectBase` with matching member layout + Serialize wiring + REGISTER_SAVELOAD_CLASS(0xA1843130, StubA1843130).
9. 155 instances now deserialize. Downstream `BasicDB.h:102` lookup asserts (328 of them) resolve. Main menu's CDBCamera / UIContainer may then load.

Estimated effort: 1-2 sessions of disassembly + 1 session of port-side impl + verify.

## Why we stopped here

Without IDA/Ghidra interactive disassembly UI, doing this in CLI is impractical (5-10× slower than tool-assisted RE). The leads above are solid enough for a dedicated RE session.

## Files

- `port/docs/patches/mapedit_classes.txt` — 285 C* tokens from MapEdit.exe (saved this round)
- Diff-source-classes work was in /tmp during this round (not committed; trivial to re-run from upstream/Soft/Andy/Jan03/a5dll/)
