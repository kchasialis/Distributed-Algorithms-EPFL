#!/usr/bin/env python3

import argparse
import os
from collections import defaultdict

def check_positive(value):
    ivalue = int(value)
    if ivalue <= 0:
        raise argparse.ArgumentTypeError(f"{value} is an invalid positive int value")
    return ivalue

def checkProcess(filePath):
    i = 1
    nextMessage = defaultdict(lambda: 1)
    filename = os.path.basename(filePath)

    with open(filePath) as f:
        for lineNumber, line in enumerate(f):
            tokens = line.split()

            # Check broadcast
            if tokens[0] == 'b':
                msg = int(tokens[1])
                if msg != i:
                    print(f"File {filename}, Line {lineNumber + 1}: Messages broadcast out of order. "
                          f"Expected message {i} but broadcast message {msg}")
                    return False
                i += 1

            # Check delivery
            if tokens[0] == 'd':
                sender = int(tokens[1])
                msg = int(tokens[2])
                if msg != nextMessage[sender]:
                    print(f"File {filename}, Line {lineNumber + 1}: Message delivered out of order. "
                          f"Expected message {nextMessage[sender]}, but delivered message {msg}")
                    return False
                else:
                    nextMessage[sender] = msg + 1

    return True

def checkLineConsistency(filePaths):
    """Ensure all files have the same number of lines."""
    line_counts = []
    for filePath in filePaths:
        with open(filePath) as f:
            line_counts.append(sum(1 for _ in f))

    if len(set(line_counts)) != 1:
        print(f"Inconsistent line counts: {line_counts}")
        return False

    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--proc_num",
        required=True,
        type=check_positive,
        dest="proc_num",
        help="Total number of processes",
    )

    results = parser.parse_args()

    # Construct output file names
    output_files = [f"{i}.output" for i in range(1, results.proc_num + 1)]

    # Check line consistency
    if not checkLineConsistency(output_files):
        print("Validation failed: Output files have inconsistent line counts.")
        exit(1)

    # Check each output file
    all_ok = True
    not_ok = []
    for o in output_files:
        if not checkProcess(o):
            all_ok = False
            not_ok.append(o)

    if all_ok:
        print("Validation passed.")
    else:
        print("Validation failed.")
        print("Files that failed validation:")
        for o in not_ok:
            print(f"  {o}")
        exit(1)
