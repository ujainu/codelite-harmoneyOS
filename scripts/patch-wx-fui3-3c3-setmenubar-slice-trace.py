#!/usr/bin/env python3
"""F-UI-3.3c-3: DetachMenuBar blr target log — DISABLED on device.

SetMenuBar body instrumentation (mid-frame probes or blr wrapper @ 0x43FEAC)
SIGSEGV when combined with F-UI-3.3c fix. Use f-ui-3-3c3-verify.sh caller-chain
slice instead (patch-f-ui-3-1c-frame-flow-trace.py + patch-wx-fui3-3b2-detach-enter-trace.py).
"""
from __future__ import annotations

import sys


def main() -> None:
    print(
        "[f-ui-3.3c-3] SetMenuBar body trace disabled (SIGSEGV with 3.3c). "
        "Use f-ui-3-3c3-verify.sh caller-chain slice.",
        file=sys.stderr,
    )
    sys.exit(2)


if __name__ == "__main__":
    main()
