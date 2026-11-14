#!/usr/bin/env python3
"""VukangDB / VKFS data-integrity checks (v0.2.2).

  A. disk full: a table that cannot grow must survive unchanged (64 KiB disk)
  B. a failed multi-row INSERT leaves nothing behind (also inside BEGIN/ROLLBACK)
  C. UPDATE cannot break a PRIMARY KEY

Boots the kernel directly (QEMU Multiboot, text console, no splash) so it is
fast. Usage: python3 scripts/integrity_test.py [path/to/vukang.kernel]
Exit code 0 only if every check passes."""
import os, re, select, subprocess, sys, time

KERNEL = sys.argv[1] if len(sys.argv) > 1 else "build/vukang.kernel"
out = b""

def boot(disk):
    global out
    out = b""
    return subprocess.Popen(
        ["qemu-system-i386", "-m", "128M", "-kernel", KERNEL, "-append", "nosplash",
         "-drive", f"file={disk},format=raw,index=0,media=disk",
         "-display", "none", "-serial", "stdio", "-monitor", "none", "-no-reboot"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

def read_until(p, token, timeout=20):
    global out
    end = time.time() + timeout
    while time.time() < end:
        if select.select([p.stdout], [], [], 0.2)[0]:
            chunk = os.read(p.stdout.fileno(), 65536)
            if not chunk: break
            out += chunk
            if token.encode() in out: return True
    return False

def cmd(p, line, timeout=20, quiet=False):
    global out
    time.sleep(0.2)
    while select.select([p.stdout], [], [], 0.1)[0]:
        if not os.read(p.stdout.fileno(), 65536): break
    out = b""
    for ch in line + "\r":
        p.stdin.write(ch.encode()); p.stdin.flush(); time.sleep(0.002)
    read_until(p, "vukang:~$ ", timeout)
    text = out.decode(errors="replace")
    if not quiet: print(text.strip())
    return text

def count(p, table):
    m = re.search(r"COUNT = (-?\d+)", cmd(p, f"sql SELECT COUNT(*) FROM {table}"))
    return int(m.group(1)) if m else None

def halt(p):
    p.stdin.write(b"halt\r"); p.stdin.flush()
    try: p.wait(10)
    except subprocess.TimeoutExpired: p.kill(); p.wait()

checks = []
def check(name, cond):
    checks.append((name, cond)); print(("PASS " if cond else "FAIL ") + name)

def fresh_disk(path, size):
    with open(path, "wb") as f: f.truncate(size)

# ---------- A: disk full must not destroy the table ----------
DISK_A = "build/integrity-64k.img"
fresh_disk(DISK_A, 64 * 1024)
p = boot(DISK_A)
assert read_until(p, "vukang:~$ ", 30), out.decode(errors="replace")
cmd(p, "sql CREATE TABLE big (id INT PRIMARY KEY, a TEXT, b TEXT, c TEXT, d TEXT, e TEXT, f TEXT, g TEXT)")
cmd(p, "write after.txt kaa baada ya table")   # blocks in-place growth: forces relocation
nid, ok_rows, failed_at = 1, 0, None
for stmt in range(80):
    tuples = ",".join(f"({nid + k},'x','x','x','x','x','x','x')" for k in range(6))
    r = cmd(p, f"sql INSERT INTO big VALUES {tuples}", quiet=True)
    if "SQL error" in r:
        failed_at = stmt; print(r.strip()); break
    nid += 6; ok_rows += 6
n_after_error = count(p, "big")
check("A: neighbouring file intact after the error", "kaa baada" in cmd(p, "cat after.txt"))
print(f"[A] {ok_rows} rows inserted before the disk filled up (statement {failed_at})")
check("A: disk-full INSERT reports an error", failed_at is not None)
check("A: table still has every committed row after the error", n_after_error == ok_rows)
halt(p)
p = boot(DISK_A)
assert read_until(p, "vukang:~$ ", 30)
n_reboot = count(p, "big")
check("A: same row count after reboot (table not deleted)", n_reboot == ok_rows)
halt(p)
os.remove(DISK_A)

# ---------- B + C on a normal disk ----------
DISK_B = "build/integrity-16m.img"
fresh_disk(DISK_B, 16 * 1024 * 1024)
p = boot(DISK_B)
assert read_until(p, "vukang:~$ ", 30)
cmd(p, "sql CREATE TABLE t (id INT PRIMARY KEY, name TEXT)")
cmd(p, "sql INSERT INTO t VALUES (1,'a')")
r = cmd(p, "sql INSERT INTO t VALUES (2,'b'),(1,'dup')")
check("B: duplicate inside a multi-row INSERT is rejected", "duplicate PRIMARY KEY" in r)
check("B: failed INSERT leaves 1 row (all or nothing)", count(p, "t") == 1)
r = cmd(p, "sql BEGIN; INSERT INTO t VALUES (5,'x'),(1,'dup')")
cmd(p, "sql ROLLBACK")
check("B: BEGIN + failed INSERT + ROLLBACK leaves nothing", count(p, "t") == 1)

cmd(p, "sql INSERT INTO t VALUES (7,'g'),(8,'h')")
check("C: UPDATE to an existing key is rejected",
      "duplicate PRIMARY KEY" in cmd(p, "sql UPDATE t SET id = 1 WHERE id = 7"))
check("C: UPDATE to a free key succeeds",
      "1 row(s) updated" in cmd(p, "sql UPDATE t SET id = 70 WHERE id = 7"))
check("C: UPDATE of PK on many rows is rejected",
      "SQL error" in cmd(p, "sql UPDATE t SET id = 9"))
s = cmd(p, "sql SELECT id FROM t ORDER BY id")
ids = [int(x) for x in re.findall(r"^\|\s+(\d+) \|", s, re.M)]
check("C: keys still unique and unchanged by the failed UPDATEs", ids == [1, 8, 70])
halt(p)
p = boot(DISK_B)
assert read_until(p, "vukang:~$ ", 30)
check("B/C: same 3 rows after reboot", count(p, "t") == 3)
halt(p)
os.remove(DISK_B)

failed = [n for n, c in checks if not c]
print(f"\n{len(checks) - len(failed)}/{len(checks)} integrity checks passed")
sys.exit(1 if failed else 0)
