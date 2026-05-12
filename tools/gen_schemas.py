"""Static schema extractor for silent-storm DBFormat classes.

Walks upstream/Soft/Andy/Jan03/a5dll/DBFormat/*.cpp, finds every
``void <Class>::Import()`` whose body is composed solely of
``NDatabase::ImportField( "Name", &expr )`` calls, parses the
corresponding ``Data<X>.h`` to learn member types, and emits a single
header ``port/src/stubs/db_record_schemas.h`` containing per-class
``SColumnInfo`` tables suitable for compile-time ``offsetof()`` lookups.

Run from the repo root or from anywhere; paths are anchored on the
location of this script.
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

# ---------------------------------------------------------------------------
# Layout
# ---------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).resolve().parent
PORT_DIR = SCRIPT_DIR.parent
REPO_ROOT = PORT_DIR.parent
DBFORMAT_DIR = REPO_ROOT / "upstream" / "Soft" / "Andy" / "Jan03" / "a5dll" / "DBFormat"
OUTPUT_PATH = PORT_DIR / "src" / "stubs" / "db_record_schemas.h"

# Type codes per spec
T_INT = 0
T_BOOL = 1
T_FLOAT = 2
T_STRING = 3
T_WSTRING = 4
T_REF = 5

# C++ leaf-type spellings → type code (used after stripping const/refs).
PRIMITIVE_TYPE_MAP = {
    "bool": T_BOOL,
    "float": T_FLOAT,
    "double": T_FLOAT,
    "int": T_INT,
    "unsigned": T_INT,
    "long": T_INT,
    "short": T_INT,
    "DWORD": T_INT,
    "BYTE": T_INT,
    "WORD": T_INT,
    "UINT": T_INT,
    "INT": T_INT,
    "char": T_INT,
    "string": T_STRING,
    "std::string": T_STRING,
    "wstring": T_WSTRING,
    "std::wstring": T_WSTRING,
}

# Heuristic for sub-fields where we have no parsed type info (e.g. CVec3.x
# member resolved via offsetof on a struct we did not parse): the spec uses
# Hungarian prefixes throughout the source, so we read the leaf name.
SUBFIELD_NAME_HEURISTIC = [
    (re.compile(r"^b[A-Z_]"), T_BOOL),
    (re.compile(r"^f[A-Z_]"), T_FLOAT),
    (re.compile(r"^pt[A-Z_]"), T_FLOAT),
    (re.compile(r"^v[A-Z_]"), T_FLOAT),
    (re.compile(r"^sz[A-Z_]"), T_STRING),
    (re.compile(r"^wsz[A-Z_]"), T_WSTRING),
    (re.compile(r"^p[A-Z_]"), T_REF),
    (re.compile(r"^n[A-Z_]"), T_INT),
    (re.compile(r"^dw[A-Z_]"), T_INT),
]

# Hungarian-prefix fallback for outer member when header parse missed it.
NAME_TYPE_FALLBACK = [
    (re.compile(r"^b[A-Z_]"), T_BOOL),
    (re.compile(r"^f[A-Z_]"), T_FLOAT),
    (re.compile(r"^sz[A-Z_]"), T_STRING),
    (re.compile(r"^wsz[A-Z_]"), T_WSTRING),
    (re.compile(r"^p[A-Z_]"), T_REF),
    (re.compile(r"^n[A-Z_]"), T_INT),
    (re.compile(r"^dw[A-Z_]"), T_INT),
]


def code_to_comment(code: int) -> str:
    return {0: "int", 1: "bool", 2: "float", 3: "string", 4: "wstring", 5: "ref"}[code]


# ---------------------------------------------------------------------------
# Comment stripping (preserve newlines for line-based parsing).
# ---------------------------------------------------------------------------


def strip_comments(src: str) -> str:
    out = []
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            # line comment to end of line
            j = src.find("\n", i)
            if j == -1:
                break
            i = j  # keep the \n
        elif c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            if j == -1:
                break
            # Preserve newlines inside block comment so line numbers stay sane.
            block = src[i : j + 2]
            out.append("".join("\n" if ch == "\n" else " " for ch in block))
            i = j + 2
        elif c == '"':
            # string literal — copy verbatim
            j = i + 1
            while j < n:
                if src[j] == "\\" and j + 1 < n:
                    j += 2
                    continue
                if src[j] == '"':
                    j += 1
                    break
                j += 1
            out.append(src[i:j])
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


# ---------------------------------------------------------------------------
# Header parsing: collect {ClassName: {MemberName: TypeSpelling}} per .h.
# ---------------------------------------------------------------------------


@dataclass
class HeaderInfo:
    # ClassName -> { memberName -> (raw_type_spelling, type_code) }
    classes: dict
    # ClassName -> source header file name (basename only).
    class_to_header: dict


# Match a member declaration. Captures: full type plus a comma-separated list
# of names (each possibly followed by an array suffix). Examples:
#   float fYaw;
#   CPtr<CString> pText;
#   string sParam[N_ACK_MAX_PARAM_COUNT];
#   vector< CPtr<T> > vec;
#   CPtr<CTexture> pPositiveX, pPositiveY, pPositiveZ;
MEMBER_RE = re.compile(
    r"""^\s*
        (?P<type>
            (?:const\s+)?
            [A-Za-z_][\w:]*                            # base type (possibly qualified)
            (?:\s*<[^;{}]*>)?                          # optional <...> template args
            (?:\s*\*)?                                 # optional pointer
        )
        \s+
        (?P<names>
            [A-Za-z_]\w*                               # first name
            (?:\s*\[[^\]]*\])?                         # optional array
            (?:\s*,\s*[A-Za-z_]\w*(?:\s*\[[^\]]*\])?)* # additional names
        )
        \s*;
    """,
    re.VERBOSE,
)


def split_member_names(names_blob: str) -> list[str]:
    """Given the captured comma-separated 'names' chunk from MEMBER_RE, return
    the bare identifier names (drop any [N] array suffix)."""
    out = []
    for piece in names_blob.split(","):
        piece = piece.strip()
        m = re.match(r"^([A-Za-z_]\w*)", piece)
        if m:
            out.append(m.group(1))
    return out


def map_type_to_code(type_spelling: str) -> int | None:
    """Map a C++ type spelling to one of the int codes, or None if unknown."""
    t = type_spelling.strip()
    # Strip leading const
    if t.startswith("const "):
        t = t[len("const ") :].strip()
    # Strip trailing pointer/reference markers
    t = t.rstrip("*&").strip()
    # CPtr<T>, CRndPtr<T>, CDBPtr<T> all behave as references for our purposes.
    if re.match(r"^(CPtr|CRndPtr|CDBPtr|CWeakPtr)\s*<", t):
        return T_REF
    # vector<...>, list<...>, set<...> — treat as opaque container; skip.
    if re.match(r"^(vector|list|set|map|deque)\s*<", t):
        return None
    # Strip namespaces.
    base = t.split("<", 1)[0].strip()
    base = base.rsplit("::", 1)[-1]
    if base in PRIMITIVE_TYPE_MAP:
        return PRIMITIVE_TYPE_MAP[base]
    return None


# r32: when a member is declared `T name[N]` and ImportField targets &name[i],
# we need to inspect the element type to determine the column kind. The
# header parser already records the full declared type spelling (T) without
# the [N] suffix (we use split on whitespace then take the type part), so
# we can reuse map_type_to_code directly on the recorded type.


# CPtr<X>, CRndPtr<X>, CDBPtr<X>, CWeakPtr<X> -> inner class name X (stripped
# of namespace prefix like NDb::). Returns None if not a smart-pointer type.
SMARTPTR_INNER_RE = re.compile(
    r"^(?:CPtr|CRndPtr|CDBPtr|CWeakPtr)\s*<\s*([A-Za-z_][\w:]*)\s*>$",
)


def extract_ref_inner_class(type_spelling: str) -> str | None:
    t = type_spelling.strip()
    if t.startswith("const "):
        t = t[len("const ") :].strip()
    t = t.rstrip("*&").strip()
    m = SMARTPTR_INNER_RE.match(t)
    if not m:
        return None
    inner = m.group(1).strip()
    # Strip namespace prefixes (e.g. NDb::CTexture -> CTexture).
    inner = inner.rsplit("::", 1)[-1]
    return inner


CLASS_HEAD_RE = re.compile(
    r"\b(?:class|struct)\s+([A-Za-z_]\w*)\b(?:\s*:\s*(?:public|protected|private)[^\{;]*)?\s*\{",
)


def find_class_bodies(src: str) -> list[tuple[str, int, int]]:
    """Return [(class_name, body_start, body_end_exclusive), ...].

    body_start points to the char AFTER the opening '{'.
    body_end_exclusive points to the matching '}' position.
    """
    results = []
    for m in CLASS_HEAD_RE.finditer(src):
        name = m.group(1)
        open_brace = m.end() - 1
        depth = 1
        i = open_brace + 1
        n = len(src)
        while i < n and depth > 0:
            c = src[i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    results.append((name, open_brace + 1, i))
                    break
            elif c == '"':
                # skip string
                j = i + 1
                while j < n and src[j] != '"':
                    if src[j] == "\\" and j + 1 < n:
                        j += 2
                        continue
                    j += 1
                i = j
            i += 1
    return results


def parse_header(path: Path) -> HeaderInfo:
    src = path.read_text(encoding="utf-8", errors="replace")
    src = strip_comments(src)
    info = HeaderInfo(classes={}, class_to_header={})
    for cls_name, body_start, body_end in find_class_bodies(src):
        body = src[body_start:body_end]
        # Skip nested classes' inner contents by remembering top-level only:
        # We re-scan body excluding nested {...} blocks for member regex matches.
        members = {}
        # Remove nested braces so we don't pick up inline-function bodies.
        cleaned = remove_nested_braces(body)
        for line in cleaned.splitlines():
            mm = MEMBER_RE.match(line)
            if not mm:
                continue
            tname = re.sub(r"\s+", " ", mm.group("type")).strip()
            names_blob = mm.group("names")
            # Skip obvious non-members (typedef, using, friend, etc. already
            # filtered by regex). Also reject all-caps macro-ish lines.
            if tname in {"return", "case", "default"}:
                continue
            for mname in split_member_names(names_blob):
                members[mname] = tname
        info.classes[cls_name] = members
        info.class_to_header[cls_name] = path.name
    return info


def remove_nested_braces(body: str) -> str:
    """Replace nested {...} regions with empty space to avoid matching members
    inside inline function bodies."""
    out = []
    depth = 0
    for ch in body:
        if ch == "{":
            depth += 1
            out.append(" ")
        elif ch == "}":
            depth -= 1
            out.append(" ")
        else:
            if depth > 0:
                out.append(" " if ch != "\n" else "\n")
            else:
                out.append(ch)
    return "".join(out)


# ---------------------------------------------------------------------------
# .cpp parsing: find Import() bodies, extract clean ImportField calls.
# ---------------------------------------------------------------------------

IMPORT_FN_RE = re.compile(
    r"void\s+([A-Za-z_]\w*)\s*::\s*Import\s*\(\s*\)\s*\{",
)

# REGISTER_SAVELOAD_CLASS( 0xHEX, ClassName )
# REGISTER_SAVELOAD_CLASS_NM( 0xHEX, ClassName, NDb )
# Be flexible about whitespace and the optional _NM suffix.
REGISTER_SAVELOAD_RE = re.compile(
    r"""\bREGISTER_SAVELOAD_CLASS(?P<nm>_NM)?\s*\(\s*
        (?P<typeid>0[xX][0-9A-Fa-f]+)\s*,\s*
        (?P<cls>[A-Za-z_]\w*)
        (?:\s*,\s*(?P<ns>[A-Za-z_]\w*))?
        \s*\)
    """,
    re.VERBOSE,
)

# REGISTER_DATABASE_CLASS( N, "TableName", ClassName )
# REGISTER_DATABASE_CLASS_NM( N, "TableName", ClassName, NDb )
# REGISTER_DATABASE_CLASS_TEMPL( N, "TableName", ClassName, OtherClass )
# N may be decimal (e.g. 1, 26) or hex (e.g. 0xE0000001).
REGISTER_DATABASE_RE = re.compile(
    r"""\bREGISTER_DATABASE_CLASS(?P<suffix>_NM|_TEMPL)?\s*\(\s*
        (?P<tableid>0[xX][0-9A-Fa-f]+|\d+)\s*,\s*
        "(?P<tablename>[^"\\]*)"\s*,\s*
        (?P<cls>[A-Za-z_]\w*)
        (?:\s*,\s*(?P<extra>[A-Za-z_]\w*))?
        \s*\)
    """,
    re.VERBOSE,
)

# r32: relaxed ImportField extraction — finds every ImportField call in the
# body regardless of surrounding control flow (loops, if-conditions, etc.).
# Patterns recognized for the address expression after '&':
#   outer
#   outer.sub
#   outer[N]          -> array element (N must be a literal integer)
#   outer[N].sub
# Anything else (function call, ptr-arith, casts, []s with non-literal index)
# is skipped silently — the class still gets a partial schema instead of
# being rejected outright.
IMPORTFIELD_STMT_RE = re.compile(
    r"""\bNDatabase\s*::\s*ImportField\s*\(\s*
        "(?P<colname>[^"\\]*)"\s*,\s*
        &\s*(?P<outer>[A-Za-z_]\w*)
        (?:\s*\[\s*(?P<idx>\d+)\s*\])?
        (?:\s*\.\s*(?P<sub>[A-Za-z_]\w*))?
        \s*\)\s*;
    """,
    re.VERBOSE,
)


@dataclass
class ImportCall:
    colname: str
    outer: str
    sub: str | None
    idx: int | None = None  # r32: array index for outer[idx] expressions


@dataclass
class ParsedImport:
    class_name: str
    fields: list  # list[ImportCall]
    cpp_file: str
    line_no: int


@dataclass
class SkippedImport:
    class_name: str
    cpp_file: str
    line_no: int
    reason: str


def find_balanced_close(src: str, open_pos: int) -> int:
    """Given index of '{', return index of matching '}'. -1 if unbalanced."""
    depth = 0
    i = open_pos
    n = len(src)
    while i < n:
        c = src[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i
        elif c == '"':
            j = i + 1
            while j < n:
                if src[j] == "\\" and j + 1 < n:
                    j += 2
                    continue
                if src[j] == '"':
                    break
                j += 1
            i = j
        i += 1
    return -1


def split_statements(body: str) -> list[str]:
    """Split a function body into top-level statements terminated by ';'.
    Returns trimmed statement strings (no trailing ';'). Skips empty and
    pure-whitespace fragments. Statements that contain nested braces or
    parentheses keep them as-is (we only split on top-level ';')."""
    out = []
    depth_paren = 0
    depth_brace = 0
    start = 0
    n = len(body)
    i = 0
    while i < n:
        c = body[i]
        if c == "(":
            depth_paren += 1
        elif c == ")":
            depth_paren -= 1
        elif c == "{":
            depth_brace += 1
        elif c == "}":
            depth_brace -= 1
        elif c == '"':
            j = i + 1
            while j < n:
                if body[j] == "\\" and j + 1 < n:
                    j += 2
                    continue
                if body[j] == '"':
                    break
                j += 1
            i = j
        elif c == ";" and depth_paren == 0 and depth_brace == 0:
            stmt = body[start : i + 1].strip()
            if stmt:
                out.append(stmt)
            start = i + 1
        i += 1
    tail = body[start:].strip()
    if tail:
        out.append(tail)
    return out


def parse_saveload_registrations(src: str) -> dict:
    """Return {ClassName: typeID_int} for every REGISTER_SAVELOAD_CLASS(_NM)
    match in the given (comment-stripped) source."""
    out = {}
    for m in REGISTER_SAVELOAD_RE.finditer(src):
        cls = m.group("cls")
        try:
            tid = int(m.group("typeid"), 16)
        except ValueError:
            continue
        # If a class is registered more than once (shouldn't happen), keep first.
        out.setdefault(cls, tid)
    return out


def parse_database_registrations(src: str) -> dict:
    """Return {ClassName: (nTableID_int, tableName)} for every
    REGISTER_DATABASE_CLASS(_NM|_TEMPL) match in the given (comment-stripped)
    source. nTableID may be small decimal (e.g. 1, 26) or hex (e.g. 0xE0000001).
    """
    out = {}
    for m in REGISTER_DATABASE_RE.finditer(src):
        cls = m.group("cls")
        raw = m.group("tableid")
        try:
            tid = int(raw, 16) if raw.lower().startswith("0x") else int(raw, 10)
        except ValueError:
            continue
        out.setdefault(cls, (tid, m.group("tablename")))
    return out


def parse_cpp(path: Path):
    """Return (parsed, skipped, regs, db_regs).

    regs: {ClassName: saveload_typeID_int}
    db_regs: {ClassName: (nTableID_int, tableName_str)}
    """
    raw = path.read_text(encoding="utf-8", errors="replace")
    src = strip_comments(raw)
    parsed = []
    skipped = []
    regs = parse_saveload_registrations(src)
    db_regs = parse_database_registrations(src)
    for m in IMPORT_FN_RE.finditer(src):
        cls = m.group(1)
        open_brace = m.end() - 1
        close_brace = find_balanced_close(src, open_brace)
        if close_brace < 0:
            skipped.append(
                SkippedImport(cls, path.name, src.count("\n", 0, open_brace) + 1, "unbalanced braces")
            )
            continue
        body = src[open_brace + 1 : close_brace]
        line_no = src.count("\n", 0, open_brace) + 1
        # r32: scan whole body for all ImportField calls — even ones inside
        # `if (IsValid(...))` blocks or `for (...)` loops — and dedupe
        # by column name (keep first occurrence). Non-matching code is
        # silently ignored (we just take what we recognise).
        # Track imports keyed by (cname) so per-class dedupe works.
        raw_calls = []  # list of (cname, outer, sub, idx)
        seen_cols = set()
        for sm in IMPORTFIELD_STMT_RE.finditer(body):
            cname = sm.group("colname")
            if cname in seen_cols:
                continue
            seen_cols.add(cname)
            idx_str = sm.group("idx")
            raw_calls.append(
                (
                    cname,
                    sm.group("outer"),
                    sm.group("sub"),
                    int(idx_str) if idx_str is not None else None,
                )
            )
        # Defer member-presence filtering to caller (needs header_infos).
        calls = raw_calls
        if not calls:
            skipped.append(SkippedImport(cls, path.name, line_no, "no ImportField calls found"))
            continue
        parsed.append(
            ParsedImport(cls, [ImportCall(*c) for c in calls], path.name, line_no)
        )
    return parsed, skipped, regs, db_regs


# ---------------------------------------------------------------------------
# Type inference using header info + heuristics.
# ---------------------------------------------------------------------------


def name_fallback_type(name: str) -> int:
    for rx, code in NAME_TYPE_FALLBACK:
        if rx.match(name):
            return code
    return T_INT  # last resort — better than nothing


def subfield_name_to_code(name: str) -> int:
    for rx, code in SUBFIELD_NAME_HEURISTIC:
        if rx.match(name):
            return code
    # Single-letter leaf names like x/y/z on vector types: assume float.
    if len(name) == 1 and name in "xyzwrgbauv":
        return T_FLOAT
    if name in {"left", "top", "right", "bottom"}:
        return T_INT
    return T_INT


def resolve_type_code(
    class_name: str,
    outer_member: str,
    sub_member: str | None,
    headers: list,
) -> int:
    """Look up the member declared in class_name's parsed header info."""
    decl_type = None
    for hi in headers:
        if class_name in hi.classes:
            members = hi.classes[class_name]
            if outer_member in members:
                decl_type = members[outer_member]
                break
    # Compute outer-level type code.
    if sub_member is None:
        if decl_type is not None:
            code = map_type_to_code(decl_type)
            if code is not None:
                return code
        return name_fallback_type(outer_member)
    # Sub-field: code depends on the SUB member's type.
    # We don't aggressively resolve the outer struct here; the Hungarian
    # naming on the sub member is the strongest signal we have.
    return subfield_name_to_code(sub_member)


def resolve_ref_inner_class(
    class_name: str, outer_member: str, headers: list
) -> str | None:
    """If the named member is a CPtr<X>-style field, return X (the target
    record class). Returns None otherwise."""
    for hi in headers:
        if class_name in hi.classes:
            members = hi.classes[class_name]
            if outer_member in members:
                return extract_ref_inner_class(members[outer_member])
    return None


def resolve_outer_struct_type(
    class_name: str, outer_member: str, headers: list
) -> str | None:
    """Return a string like 'CVec3' or 'CTPoint<int>' to be used inside
    offsetof(<struct>, sub). Preserves template arguments so offsetof works
    on instantiated class templates (e.g. CTPoint<int>). Returns None if we
    can't tell, in which case the caller falls back to a zero subOffset."""
    for hi in headers:
        if class_name in hi.classes:
            members = hi.classes[class_name]
            if outer_member in members:
                t = members[outer_member].strip()
                # Strip const/pointer markers.
                if t.startswith("const "):
                    t = t[6:].strip()
                t = t.rstrip("*&").strip()
                # Strip namespace prefix from the leading identifier, but
                # KEEP any '<...>' template args appended.
                lt = t.find("<")
                if lt < 0:
                    base = t.rsplit("::", 1)[-1]
                else:
                    head = t[:lt].rsplit("::", 1)[-1]
                    base = head + t[lt:]
                # Normalize internal whitespace.
                base = re.sub(r"\s+", " ", base).strip()
                if base:
                    return base
    return None


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------


def main() -> int:
    if not DBFORMAT_DIR.is_dir():
        print(f"DBFormat dir not found: {DBFORMAT_DIR}", file=sys.stderr)
        return 1

    cpp_files = sorted(DBFORMAT_DIR.glob("Data*.cpp"))
    h_files = sorted(DBFORMAT_DIR.glob("Data*.h"))

    # Parse all headers up front.
    header_infos = [parse_header(h) for h in h_files]

    # Build a map: class -> header basename, for include-list generation.
    class_to_header = {}
    for hi in header_infos:
        class_to_header.update(hi.class_to_header)

    # Build a global merged class -> members map for membership tests.
    # Note: a class may appear in multiple headers via fwd decls. Merge.
    merged_members = {}  # cls -> set of member names
    for hi in header_infos:
        for cls, mems in hi.classes.items():
            merged_members.setdefault(cls, set()).update(mems.keys())

    # Walk the class hierarchy: a class that inherits from another can also
    # call ImportField on the parent's members. Build a base-class map.
    # The current header parser doesn't track inheritance; do a quick
    # second-pass regex grep of the headers for `class X : ... public Y`.
    CLASS_INHERIT_RE = re.compile(
        r"\bclass\s+([A-Za-z_]\w*)\b\s*:\s*(?:public|protected|private)\s+([A-Za-z_:][\w:]*)",
    )
    base_classes = {}  # cls -> base cls (immediate)
    for h in h_files:
        try:
            src_h = strip_comments(h.read_text(encoding="utf-8", errors="replace"))
        except Exception:
            continue
        for m in CLASS_INHERIT_RE.finditer(src_h):
            child = m.group(1)
            base = m.group(2).rsplit("::", 1)[-1]
            base_classes.setdefault(child, base)

    def class_has_member(cls, name):
        """Walk class + bases, return True if `name` is declared anywhere."""
        cur = cls
        seen = set()
        while cur and cur not in seen:
            seen.add(cur)
            if name in merged_members.get(cur, ()):
                return True
            cur = base_classes.get(cur)
        return False

    def member_decl_type(cls, name):
        """Return the recorded type spelling of `cls::name`, walking bases."""
        cur = cls
        seen = set()
        while cur and cur not in seen:
            seen.add(cur)
            for hi in header_infos:
                mems = hi.classes.get(cur)
                if mems and name in mems:
                    return mems[name]
            cur = base_classes.get(cur)
        return None

    CONTAINER_RE = re.compile(r"^(?:const\s+)?(?:std::)?(vector|list|set|map|deque)\s*<")

    def member_is_container(cls, name):
        t = member_decl_type(cls, name)
        if not t:
            return False
        return bool(CONTAINER_RE.match(t.strip()))

    all_parsed = []
    all_skipped = []
    all_regs = {}  # ClassName -> saveload typeID (int)
    all_db_regs = {}  # ClassName -> (nTableID, tableName)
    # Scan all Data*.cpp for both Import bodies AND REGISTER_DATABASE_CLASS.
    # In practice REGISTER_DATABASE_CLASS lives in DataFormat.cpp; scanning all
    # is cheap and future-proof.
    for cpp in cpp_files:
        parsed, skipped, regs, db_regs = parse_cpp(cpp)
        all_parsed.extend(parsed)
        all_skipped.extend(skipped)
        for cls, tid in regs.items():
            all_regs.setdefault(cls, tid)
        for cls, info in db_regs.items():
            all_db_regs.setdefault(cls, info)

    # r32: drop any ImportField whose outer identifier isn't a member of the
    # owning class (or any base) — those are local-variable imports the
    # game uses for scratch / unpack patterns (e.g. `string szFlags;
    # ImportField("Flags", &szFlags); UnpackVariantFlags(szFlags,...)`).
    # Also drop indexed access into vector/list members — offsetof can't
    # reach inside a heap container.
    dropped_local_count = 0
    dropped_container_idx_count = 0
    for p in all_parsed:
        kept = []
        for c in p.fields:
            if not class_has_member(p.class_name, c.outer):
                dropped_local_count += 1
                continue
            if c.idx is not None and member_is_container(p.class_name, c.outer):
                dropped_container_idx_count += 1
                continue
            kept.append(c)
        p.fields = kept

    # Emit the header.
    lines = []
    lines.append("// AUTO-GENERATED by tools/gen_schemas.py - do not edit")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <cstddef>")
    lines.append("")
    # Compute required headers (those whose classes appear in parsed list).
    needed_headers = set()
    for p in all_parsed:
        hdr = class_to_header.get(p.class_name)
        if hdr:
            needed_headers.add(hdr)
    rel_prefix = "../../../upstream/Soft/Andy/Jan03/a5dll/DBFormat/"
    for hdr in sorted(needed_headers):
        lines.append(f'#include "{rel_prefix}{hdr}"')
    lines.append("")
    lines.append("namespace NDb {")
    lines.append("")
    lines.append("struct SColumnInfo {")
    lines.append("    const char* name;")
    lines.append("    int type;       // 0=int, 1=bool, 2=float, 3=string, 4=wstring, 5=ref(CPtr<T>)")
    lines.append("    int offset;     // offsetof(class, outer_member)")
    lines.append("    int subOffset;  // offsetof(struct, sub_member) for nested fields, else 0")
    lines.append("    const char* refClass; // for type=5, target record class name (e.g. \"CTexture\"); else nullptr")
    lines.append("};")
    lines.append("")
    lines.append("template<typename T> const SColumnInfo* GetSchemaFor();")
    lines.append("")

    total_cols = 0
    for p in all_parsed:
        cls = p.class_name
        lines.append(
            f"// {cls}  (from {p.cpp_file}:{p.line_no})"
        )
        lines.append(
            f"template<> inline const SColumnInfo* GetSchemaFor<NDb::{cls}>() {{"
        )
        lines.append("    static const SColumnInfo cols[] = {")
        # Compute column width for tidy formatting.
        max_name_len = max((len(c.colname) for c in p.fields), default=0)
        for call in p.fields:
            tcode = resolve_type_code(cls, call.outer, call.sub, header_infos)
            name_lit = f'"{call.colname}",'
            name_field_width = max_name_len + 3  # quotes + trailing comma
            name_lit_padded = name_lit.ljust(name_field_width)
            # r32: outer may include [idx] subscript. We bake the array index
            # into the outer offset using a (char*)&p->outer[idx] - (char*)p
            # cast — works because the result is constexpr-foldable for
            # POD types and small literal indices.
            if call.idx is not None:
                offset_expr = (
                    f"(int)((char*)&((NDb::{cls}*)1)->{call.outer}[{call.idx}] - (char*)1)"
                )
            else:
                offset_expr = f"offsetof(NDb::{cls}, {call.outer})"
            if call.sub is None:
                sub_expr = "0"
            else:
                outer_struct = resolve_outer_struct_type(cls, call.outer, header_infos)
                if outer_struct is None:
                    sub_expr = f"/* unknown outer type for {call.outer} */ 0"
                else:
                    sub_expr = f"offsetof({outer_struct}, {call.sub})"
            ref_expr = "nullptr"
            if tcode == T_REF and call.sub is None:
                inner = resolve_ref_inner_class(cls, call.outer, header_infos)
                if inner is not None:
                    ref_expr = f'"{inner}"'
            lines.append(
                f"        {{ {name_lit_padded} {tcode}, {offset_expr}, {sub_expr}, {ref_expr} }},  // {code_to_comment(tcode)}"
            )
            total_cols += 1
        lines.append("        { nullptr, 0, 0, 0, nullptr }  // sentinel")
        lines.append("    };")
        lines.append("    return cols;")
        lines.append("}")
        lines.append("")

    # --- Registry: SSchemaEntry table mapping nTableID -> schema getter ---
    # Key insight: NDatabase::tables is keyed by the small int / hex constant
    # passed as the FIRST arg to REGISTER_DATABASE_CLASS (e.g. 1 = Models,
    # 26 = SolidModels, 0xE0000001 = RPGWeaponTypes). That is a DIFFERENT
    # keyspace from the REGISTER_SAVELOAD_CLASS typeIDs we used pre-r29.
    # PromoteRecordsFromStorage iterates by nTableID, so the registry MUST be
    # keyed by nTableID for GetTable(typeID) lookups to succeed.
    parsed_set = {p.class_name for p in all_parsed}
    parsed_by_class = {p.class_name: p for p in all_parsed}
    registry_entries = []  # list of (cls, nTableID, tableName)
    db_without_schema = []  # have REGISTER_DATABASE_CLASS but no Import schema
    schema_without_db = []  # have Import schema but no REGISTER_DATABASE_CLASS

    for cls, (tid, tname) in all_db_regs.items():
        if cls not in parsed_set:
            db_without_schema.append((cls, tid, tname))
            continue
        registry_entries.append((cls, tid, tname))
    for p in all_parsed:
        if p.class_name not in all_db_regs:
            schema_without_db.append(p.class_name)

    # Stable sort by nTableID for determinism.
    registry_entries.sort(key=lambda x: x[1])

    lines.append("// ---------------------------------------------------------------------------")
    lines.append("// Schema registry: nTableID -> schema getter")
    lines.append("// nTableID is the first arg to REGISTER_DATABASE_CLASS(...) — the SAME key")
    lines.append("// used by NDatabase::tables / NDatabase::GetTable(int).")
    lines.append("// ---------------------------------------------------------------------------")
    lines.append("")
    lines.append("struct SSchemaEntry {")
    lines.append("    int typeID;       // nTableID — matches NDatabase::GetTable() key")
    lines.append("    const SColumnInfo* (*getter)();")
    lines.append("};")
    lines.append("")
    # Inline wrapper functions: one per class with a known nTableID + schema.
    emitted_wrappers = set()
    for cls, _tid, _tname in registry_entries:
        if cls in emitted_wrappers:
            continue
        emitted_wrappers.add(cls)
        lines.append(
            f"inline const SColumnInfo* getSchema{cls}() {{ return GetSchemaFor<NDb::{cls}>(); }}"
        )
    lines.append("")
    lines.append("inline const SSchemaEntry* GetAllSchemas(int* outCount) {")
    lines.append("    static const SSchemaEntry entries[] = {")
    for cls, tid, tname in registry_entries:
        lines.append(f"        {{ 0x{tid & 0xFFFFFFFF:08x}, &getSchema{cls} }},  // {tname}")
    lines.append("    };")
    lines.append("    *outCount = sizeof(entries) / sizeof(entries[0]);")
    lines.append("    return entries;")
    lines.append("}")
    lines.append("")

    # --- className -> nTableID lookup ---
    # Used by dbfill to resolve type=5 ref columns: column's refClass names a
    # record class (e.g. "CTexture"); we map that to the nTableID so dbfill
    # can call NDatabase::GetTable(targetID)->GetDBRecord(refID).
    lines.append("// ---------------------------------------------------------------------------")
    lines.append("// Lookup: record class name -> nTableID (for type=5 ref resolution)")
    lines.append("// ---------------------------------------------------------------------------")
    lines.append("")
    lines.append("inline int GetTableIDForClassName(const char* name) {")
    lines.append("    if (!name) return -1;")
    lines.append("    struct E { const char* cls; int tid; };")
    lines.append("    static const E entries[] = {")
    # Emit ALL classes with REGISTER_DATABASE_CLASS, sorted by class name for
    # determinism. Don't restrict to parsed_set — a CPtr may target a class
    # whose Import body we couldn't parse, but the table still exists.
    db_sorted = sorted(all_db_regs.items(), key=lambda kv: kv[0])
    for cls, (tid, tname) in db_sorted:
        lines.append(f'        {{ "{cls}", 0x{tid & 0xFFFFFFFF:08x} }},  // {tname}')
    lines.append("    };")
    lines.append("    for (size_t i = 0; i < sizeof(entries)/sizeof(entries[0]); ++i) {")
    lines.append("        const char* a = entries[i].cls;")
    lines.append("        const char* b = name;")
    lines.append("        while (*a && *a == *b) { ++a; ++b; }")
    lines.append("        if (*a == 0 && *b == 0) return entries[i].tid;")
    lines.append("    }")
    lines.append("    return -1;")
    lines.append("}")
    lines.append("")

    lines.append("} // namespace NDb")
    lines.append("")

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text("\n".join(lines), encoding="utf-8")

    # --- Report ---
    print(f"Wrote: {OUTPUT_PATH}")
    print(f"Classes parsed: {len(all_parsed)}")
    print(f"Total columns:  {total_cols}")
    print(f"Dropped ImportFields (local var, not class member): {dropped_local_count}")
    print(f"Dropped ImportFields (indexed access into vector/list): {dropped_container_idx_count}")
    print(f"Classes skipped: {len(all_skipped)}")
    for s in all_skipped:
        print(f"  - {s.class_name}  ({s.cpp_file}:{s.line_no})  {s.reason}")
    print(f"REGISTER_SAVELOAD_CLASS entries found: {len(all_regs)}")
    print(f"REGISTER_DATABASE_CLASS entries found: {len(all_db_regs)}")
    print(f"Registry entries emitted (matched schema + REGISTER_DATABASE_CLASS): {len(registry_entries)}")
    if db_without_schema:
        print(
            f"REGISTER_DATABASE_CLASS classes WITHOUT a parsed schema (log + skip): "
            f"{len(db_without_schema)}"
        )
        for cls, tid, tname in db_without_schema:
            print(f"  - {cls}  table={tname}  nTableID=0x{tid & 0xFFFFFFFF:08x}")
    if schema_without_db:
        print(
            f"Parsed schema classes WITHOUT a REGISTER_DATABASE_CLASS "
            f"(sub-record, not a top-level table — log + skip): {len(schema_without_db)}"
        )
        for cls in schema_without_db:
            print(f"  - {cls}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
