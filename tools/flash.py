#!/usr/bin/env python3
"""
Flash a .bin through the ST-LINK mass-storage drive and capture the boot log.

Usage: python3 tools/flash.py [build/GPIO_IOToggle.bin] [--volume /Volumes/DIS_U5A9NJ]

1. copies the .bin onto the ST-LINK drive (ST-LINK programs the flash)
2. keeps (re)opening the virtual COM port while the probe re-enumerates
3. prints everything the new firmware says until a few seconds after boot
4. reports FAIL.TXT if the ST-LINK rejected the image
5. syncs the RTC to this Mac's local time (T command), unless --no-time
"""
import glob, os, select, shutil, sys, termios, time

args = [a for a in sys.argv[1:] if not a.startswith("--")]
binfile = args[0] if args else "build/GPIO_IOToggle.bin"
vol = sys.argv[sys.argv.index("--volume") + 1] if "--volume" in sys.argv else "/Volumes/DIS_U5A9NJ"

if not os.path.isdir(vol):
    sys.exit("ST-LINK drive %s not mounted" % vol)
print("flashing %s (%d bytes) -> %s" % (binfile, os.path.getsize(binfile), vol))
shutil.copy(binfile, vol)
t0 = time.time()


def open_port():
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        return None
    try:
        fd = os.open(ports[0], os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
        a = termios.tcgetattr(fd)
        a[0] = a[1] = a[3] = 0
        a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        a[4] = a[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, a)
        return fd
    except OSError:
        return None


fd, buf, booted_at, out = None, b"", None, []
while time.time() - t0 < 120:
    if fd is None:
        fd = open_port()
        if fd is None:
            time.sleep(0.2)
            continue
    try:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r:
            buf += os.read(fd, 1024)
    except OSError:                       # probe re-enumerating
        os.close(fd)
        fd = None
        continue
    while b"\n" in buf:
        line, buf = buf.split(b"\n", 1)
        s = line.decode(errors="replace").strip()
        if "Hello from UART" in s:
            out = []                      # drop stale lines from before the reset
            booted_at = time.time()
        out.append(s)
    if booted_at and time.time() - booted_at > 4:
        break
if fd is not None:
    os.close(fd)

print("\n".join(l for l in out if l))
if os.path.exists(os.path.join(vol, "FAIL.TXT")):
    print("!! FAIL.TXT:", open(os.path.join(vol, "FAIL.TXT")).read())
print("boot log %s after %.1f s" % ("captured" if booted_at else "NOT seen", time.time() - t0))

if booted_at and "--no-time" not in sys.argv:
    port = sorted(glob.glob("/dev/cu.usbmodem*"))[0]
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = a[1] = a[3] = 0
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[4] = a[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, a)
    time.sleep(1.0 - (time.time() % 1.0))              # land on a second boundary
    cmd = time.strftime("T %Y-%m-%d %H:%M:%S\n", time.localtime())
    termios.tcflush(fd, termios.TCIFLUSH)
    os.write(fd, cmd.encode())
    reply, end = b"", time.time() + 2.0
    while time.time() < end and b"OK time set" not in reply and b"ERR" not in reply:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            reply += os.read(fd, 512)
    os.close(fd)
    print("time sync:", cmd.strip(), "->", "OK" if b"OK time set" in reply else "no reply")
