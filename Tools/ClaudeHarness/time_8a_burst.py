# Stage 8A: capture bursts timed from the log line "epilogue - riding the Vashi local" (flicker at +27.5 s, the Greeting at +33.8 s)
import datetime, re, subprocess, sys, time
LOG = r"C:\Users\niles\Documents\Unreal Projects\ThanePlatform9\Saved\Logs\ThanePlatform9.log"
here = sys.argv[1]
ride = [l for l in open(LOG, encoding="utf-8", errors="ignore").read().splitlines() if "epilogue - riding the Vashi local" in l][-1]
m = re.match(r"\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d):(\d{3})\]", ride)
t = datetime.datetime(*map(int, m.groups()[:6]), int(m.group(7)) * 1000, tzinfo=datetime.timezone.utc).timestamp()
def at(off, prefix, count, gap):
    d = t + off - time.time()
    if d > 0: time.sleep(d)
    subprocess.run(["powershell", "-NoProfile", "-File", here + r"\capture_burst.ps1", "-OutPrefix", here + "\\\\" + prefix, "-Count", str(count), "-GapMs", str(gap)])
at(27.5, "e8g_flicker", 3, 400)
at(33.8, "e8g_scare", 28, 15)
