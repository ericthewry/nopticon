#! /usr/bin/python3

"""
Compute the difference between the first and the last event in a BMP message log
"""

import sys
import json
from argparse import ArgumentParser

PEERHEADER = "PeerHeader"
TIMESTAMP = "Timestamp"
TO_MS = 1000

def main():
    parser = ArgumentParser(description = "Compute the difference between the first and the last event in a BMP message log that is read from stdin")
    min_time = None
    max_time = None
    for line in sys.stdin:
        new_ts = None
        msg = json.loads(line)
        if PEERHEADER in msg and TIMESTAMP in msg[PEERHEADER]:
            new_ts = int(msg[PEERHEADER][TIMESTAMP] * TO_MS)

        if new_ts is None:
            continue
        else:
            if min_time is None:
                min_time = new_ts
            else:
                min_time = min(min_time, new_ts)
    
            if max_time is None:
                max_time = new_ts
            else:
                max_time = max(max_time, new_ts)
    print(max_time - min_time)


if __name__ == "__main__":
    main()
