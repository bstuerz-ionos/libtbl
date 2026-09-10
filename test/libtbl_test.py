#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
import subprocess
import filecmp
import os

npass = 0
nfail = 0

def check_output(fmt):
    global npass, nfail
    result = 'result.txt'
    expected = 'expected_output/' + fmt + '_expected_output.txt'

    subprocess.check_call(["./libtbl_example", "-o", result, fmt])

    if filecmp.cmp(result, expected):
        print("Pass")
        npass = npass + 1
    else:
        output = fmt + '_output.txt'
        print("Error: Compare " + expected + " and test/" + output)
        os.rename(result, output)
        nfail = nfail + 1

def main():
    print("libtbl integration test")
    print("Check table output::")
    check_output("terminal")

    print("Check JSON output::")
    check_output("json")

    print("Check XML output::")
    check_output("xml")

    print("Check CSV output::")
    check_output("csv")

    print(f"{npass} pass, {nfail} fail")
    exit(0 if nfail == 0 else 1)

if __name__ == "__main__":
    main()
