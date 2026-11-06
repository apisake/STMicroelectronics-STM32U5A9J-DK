# Usage: python3 tools/serial_read.py /dev/cu.usbmodemXXXX <seconds>   (115200 8N1)
# macOS resets termios on close, so baud must be set in the same process that reads.
import os, sys, termios, time, select
port, secs = sys.argv[1], float(sys.argv[2])
fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
a[0] = 0; a[1] = 0; a[3] = 0                       # iflag, oflag, lflag: raw
a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL  # 8N1
a[4] = a[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, a)
termios.tcflush(fd, termios.TCIFLUSH)
end = time.time() + secs
while time.time() < end:
    r, _, _ = select.select([fd], [], [], 0.2)
    if r:
        try:
            data = os.read(fd, 512)
        except OSError as e:   # e.g. ENXIO: ST-LINK unplugged / re-enumerated
            sys.stderr.write(f"\n[serial_read] port disconnected: {e}\n")
            break
        sys.stdout.write(data.decode(errors="replace").replace("\r", "")); sys.stdout.flush()
os.close(fd)
