#!/usr/bin/env python3
"""Boot VUKANG OS in QEMU, drive the shell over serial, verify results."""
import subprocess, time, sys, os, select

ISO, DISK = "build/vukang-os.iso", "build/disk.img"
if not os.path.exists(DISK):
    subprocess.run(["dd", "if=/dev/zero", f"of={DISK}", "bs=1M", "count=16", "status=none"], check=True)

def boot():
    return subprocess.Popen(
        ["qemu-system-i386", "-m", "128M", "-cdrom", ISO,
         "-drive", f"file={DISK},format=raw,index=0,media=disk", "-boot", "d",
         "-display", "none", "-serial", "stdio", "-monitor", "none", "-no-reboot"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

out = b""
def read_until(p, token, timeout=20):
    global out
    end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([p.stdout], [], [], 0.2)
        if r:
            chunk = os.read(p.stdout.fileno(), 65536)
            if not chunk: break
            out += chunk
            if token.encode() in out: return True
    return False

def cmd(p, line, wait="vukang:~$ ", timeout=20):
    global out
    time.sleep(0.3)
    while select.select([p.stdout], [], [], 0.1)[0]:   # drain pending output
        if not os.read(p.stdout.fileno(), 65536): break
    out = b""
    for ch in line + "\r":
        p.stdin.write(ch.encode()); p.stdin.flush(); time.sleep(0.004)
    ok = read_until(p, wait, timeout)
    text = out.decode(errors="replace")
    print(text)
    return text

checks = []
def check(name, cond):
    checks.append((name, cond)); print(("PASS " if cond else "FAIL ") + name)

p = boot()
assert read_until(p, "vukang:~$ ", 30), "shell prompt never appeared:\n" + out.decode(errors="replace")
print(out.decode(errors="replace"))
check("boot to shell", True)
check("mem", "Heap" in cmd(p, "mem"))
check("file write/read", "Habari" in (cmd(p, "write test.txt Habari dunia") + cmd(p, "cat test.txt")))
cmd(p, "sql DROP TABLE t")
check("sql create", "created" in cmd(p, "sql CREATE TABLE t (id INT PRIMARY KEY, name TEXT, score INT)"))
check("sql insert", "3 row(s) inserted" in cmd(p, "sql INSERT INTO t VALUES (1,'Asha',90),(2,'Juma',70),(3,'Neema',95)"))
check("sql pk", "duplicate PRIMARY KEY" in cmd(p, "sql INSERT INTO t VALUES (1,'X',1)"))
s = cmd(p, "sql SELECT name, score FROM t WHERE score > 80 ORDER BY score DESC")
check("sql select order", "Neema" in s and "Asha" in s and "Juma" not in s and s.index("Neema") < s.index("Asha"))
check("sql avg", "AVG = 85" in cmd(p, "sql SELECT AVG(score) FROM t"))
check("sql update", "1 row(s) updated" in cmd(p, "sql UPDATE t SET score = 99 WHERE name = 'Juma'"))
check("sql txn rollback", "COUNT = 3" in cmd(p, "sql BEGIN; DELETE FROM t; ROLLBACK; SELECT COUNT(*) FROM t"))
check("ps", "clockd" in cmd(p, "ps"))
check("spawn", "spawned" in cmd(p, "spawn 2"))
time.sleep(4)
check("usermode", "Kernel survived" in cmd(p, "usermode", timeout=30))
check("error handling", "SQL error" in cmd(p, "sql SELEC * FROM t"))
p.stdin.write(b"halt\r"); p.stdin.flush()
try: p.wait(10)
except subprocess.TimeoutExpired: p.kill()

# Reboot: data must persist on disk
p = boot()
assert read_until(p, "vukang:~$ ", 30)
check("persistence: file", "Habari" in cmd(p, "cat test.txt"))
check("persistence: table", "99" in cmd(p, "sql SELECT * FROM t WHERE name = 'Juma'"))
p.kill()

failed = [n for n, c in checks if not c]
print(f"\n{len(checks) - len(failed)}/{len(checks)} checks passed")
sys.exit(1 if failed else 0)
