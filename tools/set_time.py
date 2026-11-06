# Sync the board's RTC to this Mac's local time over the ST-LINK virtual COM port.
# Usage: python3 tools/set_time.py [/dev/cu.usbmodemXXXX]
# Sends "T YYYY-MM-DD HH:MM:SS\n" exactly at a second boundary, then prints the reply.
import glob, os, select, sys, termios, time

port = sys.argv[1] if len(sys.argv) > 1 else (sorted(glob.glob("/dev/cu.usbmodem*")) or [None])[0]
if not port:
    sys.exit("no /dev/cu.usbmodem* port found - is the board connected?")

fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
a[0] = 0; a[1] = 0; a[3] = 0
a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
a[4] = a[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, a)

# wait for the next full second so the board lands on .000
time.sleep(1.0 - (time.time() % 1.0))
now = time.localtime()
cmd = time.strftime("T %Y-%m-%d %H:%M:%S\n", now)
termios.tcflush(fd, termios.TCIFLUSH)
os.write(fd, cmd.encode())
print("sent:", cmd.strip(), "->", port)

buf, end = b"", time.time() + 2.0
while time.time() < end:
    r, _, _ = select.select([fd], [], [], 0.1)
    if r:
        buf += os.read(fd, 256)
        if b"OK time set" in buf or b"ERR" in buf:
            break
os.close(fd)
for line in buf.decode(errors="replace").replace("\r", "").splitlines():
    if line.startswith(("OK", "ERR")):
        print("board:", line)
        break
else:
    print("board: (no reply - check that the clock firmware is running)")
