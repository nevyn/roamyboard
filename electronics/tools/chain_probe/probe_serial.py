#!/usr/bin/env python3
"""Logs chain_probe's serial output with timestamps and forwards lines written to a FIFO.

Usage: probe_serial.py --port /dev/cu.usbserial-X --log probe.log --fifo /tmp/probe.cmd
Then: echo p > /tmp/probe.cmd
"""
import argparse
import os
import threading
import time

import serial


def forward(fifo: str, holder: dict) -> None:
    while True:
        with open(fifo) as f:  # blocks until a writer opens it
            for line in f:
                if "port" in holder:
                    holder["port"].write((line.strip() + "\n").encode())


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--log", required=True)
    ap.add_argument("--fifo", required=True)
    args = ap.parse_args()
    if not os.path.exists(args.fifo):
        os.mkfifo(args.fifo)
    holder = {}
    threading.Thread(target=forward, args=(args.fifo, holder), daemon=True).start()
    with open(args.log, "a", buffering=1) as log:
        while True:
            try:
                holder["port"] = serial.Serial(args.port, 115200, timeout=0.2)
                log.write(time.strftime("%H:%M:%S ") + "[logger] port open\n")
                while True:
                    line = holder["port"].readline()
                    if line:
                        log.write(time.strftime("%H:%M:%S ") + line.decode(errors="replace"))
            except (serial.SerialException, OSError) as e:
                log.write(time.strftime("%H:%M:%S ") + f"[logger] port lost: {e}; retrying\n")
                holder.pop("port", None)
                time.sleep(1)


if __name__ == "__main__":
    main()
