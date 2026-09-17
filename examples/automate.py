"""Generate a WinLicense license with WLQuickGen, driven by pywinauto.

Usage:
    python automate.py <path to WLQuickGen-x86.exe> <HWID> [DD MM YYYY]

Omit the date to generate a non-expiring license.
Exit code 0 on success; the status line is printed either way.
"""
import os
import sys
import time

from pywinauto.application import Application

HWID_EDIT = 1002
NO_EXPIRY = 1006
DAY, MONTH, YEAR = 1007, 1008, 1009
GENERATE = 1011
STATUS = 1012


def is_checked(checkbox):
    """Read a checkbox state across pywinauto backends (win32 vs uia)."""
    for method in ("get_check_state", "get_toggle_state"):
        getter = getattr(checkbox, method, None)
        if getter is not None:
            return bool(getter())
    raise RuntimeError("cannot read the checkbox state")


def generate(exe_path, hwid, date=None, timeout=20):
    app = Application(backend="win32").start(exe_path)
    try:
        dlg = app.window(class_name="WLQuickGenWindow")
        dlg.wait("ready", timeout=10)

        dlg.child_window(control_id=HWID_EDIT, class_name="Edit").set_edit_text(hwid)

        checkbox = dlg.child_window(control_id=NO_EXPIRY, class_name="Button")
        if date:
            if is_checked(checkbox):
                checkbox.click()
            day, month, year = date
            dlg.child_window(control_id=DAY, class_name="Edit").set_edit_text(str(day))
            dlg.child_window(control_id=MONTH, class_name="Edit").set_edit_text(str(month))
            dlg.child_window(control_id=YEAR, class_name="Edit").set_edit_text(str(year))
        elif not is_checked(checkbox):
            checkbox.click()

        dlg.child_window(control_id=GENERATE, class_name="Button").click()

        status_ctl = dlg.child_window(control_id=STATUS, class_name="Static")
        deadline = time.time() + timeout
        status = ""
        while time.time() < deadline:
            status = status_ctl.window_text()
            if status.startswith(("OK:", "ERROR:")):
                break
            time.sleep(0.2)
        return status
    finally:
        try:
            app.window(class_name="WLQuickGenWindow").close()
        except Exception:
            app.kill()


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    exe_path, hwid = sys.argv[1], sys.argv[2]
    if not os.path.isfile(exe_path):
        print("Executable not found:", exe_path)
        return 2
    date = None
    if len(sys.argv) >= 6:
        date = (sys.argv[3], sys.argv[4], sys.argv[5])

    status = generate(exe_path, hwid, date)
    print(status)
    return 0 if status.startswith("OK:") else 1


if __name__ == "__main__":
    sys.exit(main())
