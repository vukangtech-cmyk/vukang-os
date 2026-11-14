#!/usr/bin/env python3
"""Boot VUKANG OS in QEMU, drive the shell over serial, verify results."""
import subprocess, time, sys, os, select

ISO, DISK = "build/vukang-os.iso", "build/disk.img"
if not os.path.exists(DISK):
    subprocess.run(["dd", "if=/dev/zero", f"of={DISK}", "bs=1M", "count=16", "status=none"], check=True)

def boot(kernel_args=None):
    """Boot the ISO through GRUB (graphics + animation), or - with kernel_args -
    load the kernel directly with QEMU's Multiboot loader, which gives no
    framebuffer: this exercises the VGA text-mode fallback."""
    global out
    out = b""
    media = ["-cdrom", ISO, "-boot", "d"] if kernel_args is None else \
            ["-kernel", "build/vukang.kernel", "-append", kernel_args]
    return subprocess.Popen(
        ["qemu-system-i386", "-m", "128M"] + media +
        ["-drive", f"file={DISK},format=raw,index=0,media=disk",
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
bootlog = out.decode(errors="replace")
check("boot to shell", True)
check("framebuffer console (graphics mode)", "32-bit framebuffer" in bootlog)
check("boot animation played", "boot animation:" in bootlog and "fps" in bootlog)
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
check("kernel selftest 23/23", "23 passed, 0 failed" in cmd(p, "selftest"))
check("user process (private address space)", "exited with code 42" in cmd(p, "run hello", timeout=15))
s = cmd(p, "security", timeout=40)
check("security suite 6/6 attacks blocked", "6/6 protections held" in s)
check("kernel memory read blocked", "read from KERNEL address 0x00100000" in s)
import re
def free_kib():
    m = re.search(r"(\d+) KiB free", cmd(p, "mem")); return int(m.group(1)) if m else -1
before = free_kib()
for _ in range(10): cmd(p, "run attack_read", timeout=10)
cmd(p, "run hello", timeout=15)
check("no frame leak after 11 processes", before > 0 and free_kib() == before)
s = cmd(p, "run spin &")
pid = re.search(r"\[(\d+)\] spin started", s)
check("background process preempted", pid is not None and "spin" in cmd(p, "ps"))
if pid: cmd(p, f"kill {pid.group(1)}")
time.sleep(0.5)
check("kill background process", "spin" not in cmd(p, "ps"))
check("heap integrity after stress", "integrity ok" in cmd(p, "mem"))
check("error handling", "SQL error" in cmd(p, "sql SELEC * FROM t"))
p.stdin.write(b"halt\r"); p.stdin.flush()
try: p.wait(10)
except subprocess.TimeoutExpired: p.kill()

# Reboot: data must persist on disk
p = boot()
assert read_until(p, "vukang:~$ ", 30)
check("persistence: file", "Habari" in cmd(p, "cat test.txt"))
check("persistence: table", "99" in cmd(p, "sql SELECT * FROM t WHERE name = 'Juma'"))
p.kill(); p.wait()

# Corrupt a directory entry on disk: kernel must detect it, not crash
import struct
with open(DISK, "r+b") as f:
    f.seek(512); sb = f.read(64)
    magic, ver, total, bstart, bsec, dstart, dat, files = struct.unpack("<8I", sb[:32])
    f.seek(dstart * 512 + 63 * 64)                    # last directory slot
    f.write(b"evil.bin".ljust(40, b"\0") + struct.pack("<6I", 10**6, 0xFFFFFF00, 0x7FFFFFFF, 0, 1, 0))
p = boot()
ok = read_until(p, "vukang:~$ ", 30)
check("corrupted dir entry repaired at mount", ok and "removed 1 corrupted" in out.decode(errors="replace"))
check("good data survived corruption", "Habari" in cmd(p, "cat test.txt"))
p.kill(); p.wait()

# Corrupt the superblock geometry (would overflow the bitmap): must refuse to mount
with open(DISK, "r+b") as f:
    f.seek(512 + 16); f.write(struct.pack("<I", 99999))
p = boot()
ok = read_until(p, "vukang:~$ ", 30)
check("malicious superblock rejected safely", ok and "formatted new VKFS" in out.decode(errors="replace"))
p.kill(); p.wait()

# No framebuffer (old PC / text-mode GRUB entry): text console + text splash
p = boot("")
ok = read_until(p, "vukang:~$ ", 30); log = out.decode(errors="replace")
check("text-mode fallback boots", ok and "VGA text mode 80x25" in log and "boot animation:" in log)
p.kill(); p.wait()
p = boot("nosplash")
ok = read_until(p, "vukang:~$ ", 30); log = out.decode(errors="replace")
check("nosplash skips the animation", ok and "boot animation:" not in log)
p.kill()

failed = [n for n, c in checks if not c]
print(f"\n{len(checks) - len(failed)}/{len(checks)} checks passed")
os.remove(DISK)
sys.exit(1 if failed else 0)
