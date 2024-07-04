#!/usr/bin/env python3

import os
import sys

# get parameters from command line
if len(sys.argv) != 2:
  print("Usage: python3 build.py <test_name>")
  sys.exit(1)
test_name = sys.argv[1]
if test_name not in ['burst', 'scatter']:
  print("Invalid test name. Use 'burst' or 'scatter'")
  sys.exit(1)


random_numbers = [5, 10, 15]
islands = [
  [1,2,3,4,5,6,7],
  [125,126,127,128,131,132,133,134,135],
  [104,105,106,107,109,110,111]
]
filename = "app.c"

os.system('make clean')

for n in random_numbers:
  print(f'Compiling with random seed number {n}')

  # write first line of a file and keep the remaining file
  with open(filename, 'r') as f:
    lines = f.readlines()
  with open(filename, 'w') as f:
    f.write(f'#define RANDOM_SEED_NUMBER {n}\n')
    f.writelines(lines[1:])

  os.system('make TARGET=zoul')

  build_dir = f'build/{test_name}_s{n}'

  # mkdir if not exists
  os.system(f'mkdir -p {build_dir}')

  # move test.log and test_dc.log in results subfolder
  os.system(f'mv app.bin {build_dir}')

  for i, island in enumerate(islands):
    job_text = \
"""{
  "name": "%s_s%s_%s",
  "island": "DEPT",
  "start_time": "asap",
  "duration": 180,
  "binaries": {
    "hardware": "firefly",
    "bin_file": "app.bin",
    "targets": %s,
    "programAddress": "0x00200000"
  }
}""" % (test_name, n, i+1, island)

    print(job_text)
    with open(f"{build_dir}/jobFile{i+1}.json", 'w') as f:
      f.write(job_text)
