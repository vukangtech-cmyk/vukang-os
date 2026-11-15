#!/usr/bin/env python3
"""VukangDB write-ahead log: power-cut tests (v0.3).

The kernel command `walcrash <n>` stops the machine dead at a chosen moment
inside the next COMMIT. We then boot the same disk again and check what the
database looks like after recovery:

  point 1  before anything is logged            -> old data
  point 2  log saved, no table written          -> new data (log replayed)
  point 3  log saved, first table torn in half  -> new data (log replayed)
  point 4  all tables written, log not removed  -> new data (replay is harmless)
  point 5  the log itself is corrupt / torn     -> old data (log ignored)

A COMMIT that touches two tables must always leave BOTH old or BOTH new.
Usage: python3 scripts/wal_test.py [path/to/vukang.kernel]"""
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
        if token.encode() in out: return True
        if select.select([p.stdout], [], [], 0.2)[0]:
            chunk = os.read(p.stdout.fileno(), 65536)
            if not chunk: break
            out += chunk
    return token.encode() in out

def send(p, line):
    global out
    time.sleep(0.2)
    while select.select([p.stdout], [], [], 0.1)[0]:
        if not os.read(p.stdout.fileno(), 65536): break
    out = b""
    for ch in line + "\r":
        p.stdin.write(ch.encode()); p.stdin.flush(); time.sleep(0.002)

def cmd(p, line, timeout=20):
    send(p, line); read_until(p, "vukang:~$ ", timeout)
    return out.decode(errors="replace")

def count(p, table):
    m = re.search(r"COUNT = (-?\d+)", cmd(p, f"sql SELECT COUNT(*) FROM {table}"))
    return int(m.group(1)) if m else None

def power_cut(p, line):
    """Run a statement that is expected to hit the crash point; then 'pull the plug'."""
    send(p, line)
    hit = read_until(p, "WAL CRASH POINT", 30)
    p.kill(); p.wait()
    return hit

def reboot(disk):
    p = boot(disk)
    assert read_until(p, "vukang:~$ ", 30), out.decode(errors="replace")
    return p, out.decode(errors="replace")

def halt(p):
    p.stdin.write(b"halt\r"); p.stdin.flush()
    try: p.wait(10)
    except subprocess.TimeoutExpired: p.kill(); p.wait()

checks = []
def check(name, cond):
    checks.append((name, cond)); print(("PASS " if cond else "FAIL ") + name)

def fresh(path):
    with open(path, "wb") as f: f.truncate(16 * 1024 * 1024)
    return path

DISK = "build/wal-test.img"
EXPECT_NEW = {1: False, 2: True, 3: True, 4: True, 5: False}
BOOT_MSG   = {1: None, 2: "replayed 1 table", 3: "replayed 1 table", 4: "replayed 1 table", 5: "incomplete log ignored"}

# ---------- 0. normal operation leaves no log behind ----------
fresh(DISK)
p, _ = reboot(DISK)
cmd(p, "sql CREATE TABLE t (id INT PRIMARY KEY, name TEXT)")
cmd(p, "sql INSERT INTO t VALUES (1,'a'),(2,'b'),(3,'c')")
check("0: normal COMMIT removes the log (.wal)", ".wal" not in cmd(p, "ls"))
check("0: walcrash shows help", "power cut" in cmd(p, "walcrash").lower() or "1 before" in out.decode(errors="replace"))
halt(p)

# ---------- 1. single-table COMMIT cut at every point ----------
for pt in (1, 2, 3, 4, 5):
    fresh(DISK)
    p, _ = reboot(DISK)
    cmd(p, "sql CREATE TABLE t (id INT PRIMARY KEY, name TEXT)")
    cmd(p, "sql INSERT INTO t VALUES (1,'a'),(2,'b'),(3,'c')")
    cmd(p, f"walcrash {pt}")
    hit = power_cut(p, "sql INSERT INTO t VALUES (100,'new')")
    check(f"1.{pt}: power cut was simulated", hit)
    p, log = reboot(DISK)
    n = count(p, "t")
    want = 4 if EXPECT_NEW[pt] else 3
    check(f"1.{pt}: after recovery the table has {want} rows (all-or-nothing)", n == want)
    if BOOT_MSG[pt]: check(f"1.{pt}: boot log says '{BOOT_MSG[pt]}'", BOOT_MSG[pt] in log)
    check(f"1.{pt}: table is readable, not corrupted", "corrupted" not in log and "(100, " not in log and "row(s)" in cmd(p, "sql SELECT * FROM t"))
    check(f"1.{pt}: log removed after recovery", ".wal" not in cmd(p, "ls"))
    cmd(p, "sql INSERT INTO t VALUES (200,'later')")
    check(f"1.{pt}: database keeps working afterwards", count(p, "t") == want + 1)
    halt(p)

# ---------- 2. two-table COMMIT: both old or both new, never one of each ----------
for pt in (1, 2, 3, 4, 5):
    fresh(DISK)
    p, _ = reboot(DISK)
    cmd(p, "sql CREATE TABLE a (id INT PRIMARY KEY, v TEXT)")
    cmd(p, "sql CREATE TABLE b (id INT PRIMARY KEY, v TEXT)")
    cmd(p, "sql INSERT INTO a VALUES (1,'x')"); cmd(p, "sql INSERT INTO b VALUES (1,'y')")
    cmd(p, f"walcrash {pt}")
    hit = power_cut(p, "sql BEGIN; INSERT INTO a VALUES (2,'x2'); INSERT INTO b VALUES (2,'y2'); COMMIT")
    p, log = reboot(DISK)
    ca, cb = count(p, "a"), count(p, "b")
    want = 2 if EXPECT_NEW[pt] else 1
    check(f"2.{pt}: both tables {'new' if want == 2 else 'old'} after recovery (a={ca}, b={cb})", hit and ca == want and cb == want)
    halt(p)

# ---------- 3. junk in .wal must not hurt ----------
fresh(DISK)
p, _ = reboot(DISK)
cmd(p, "sql CREATE TABLE t (id INT PRIMARY KEY, name TEXT)")
cmd(p, "sql INSERT INTO t VALUES (1,'a')")
cmd(p, "write .wal this is not a valid log")
halt(p)
p, log = reboot(DISK)
check("3: garbage .wal is ignored and removed at boot", ".wal" not in cmd(p, "ls") and count(p, "t") == 1)
halt(p)

# ---------- 4. a full disk refuses the COMMIT cleanly ----------
DISK_S = "build/wal-small.img"
with open(DISK_S, "wb") as f: f.truncate(64 * 1024)
p, _ = reboot(DISK_S)
cmd(p, "sql CREATE TABLE t (id INT PRIMARY KEY, name TEXT)")
ok_rows, refused = 0, False
for b in range(200):
    vals = ",".join(f"({b*12+i+1},'row')" for i in range(12))
    if "SQL error" in cmd(p, f"sql INSERT INTO t VALUES {vals}", 30): refused = True; break
    ok_rows += 12
check("4: full disk -> COMMIT refused with an error", refused and ok_rows > 0)
check("4: no half-written log left behind", ".wal" not in cmd(p, "ls"))
check("4: table still has all committed rows", count(p, "t") == ok_rows)
halt(p)
p, log = reboot(DISK_S)
check("4: same rows after reboot", count(p, "t") == ok_rows)
halt(p)

for f in (DISK, DISK_S):
    if os.path.exists(f): os.remove(f)
failed = [n for n, c in checks if not c]
print(f"\n{len(checks) - len(failed)}/{len(checks)} WAL checks passed")
sys.exit(1 if failed else 0)
