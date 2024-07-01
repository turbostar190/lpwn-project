#!/usr/bin/env python3

import os
import time
import sys

random_numbers = [5, 10, 15]
simulations = [2, 5, 10, 20, 30, 50]

# get parameters from command line
if len(sys.argv) != 2:
  print("Usage: python3 run.py <test_name>")
  sys.exit(1)
test_name = sys.argv[1]
if test_name not in ['burst', 'scatter']:
  print("Invalid test name. Use 'burst' or 'scatter'")
  sys.exit(1)


# get now datetime as YYMMDD_HHMMSS
now = time.strftime('%y%m%d_%H%M%S')
filename = "app.c"

for sim in simulations:
  print(f'Running simulation {sim}')
  #write first line of a file and keep the remaining file

  for n in random_numbers:
    print(f'Compiling with random seed number {n}')
    # write first line of a file and keep the remaining file
    with open(filename, 'r') as f:
      lines = f.readlines()
    with open(filename, 'w') as f:
      f.write(f'#define RANDOM_SEED_NUMBER {n}\n')
      f.writelines(lines[1:])

    os.system('make')

    # run
    os.system(f'cooja_nogui nd-test-mrm-{sim}n.csc')

    results_dir = f'results/{test_name}_{sim}_s{n}'

    # mkdir if not exists
    os.system(f'mkdir -p {results_dir}')

    # move test.log and test_dc.log in results subfolder
    os.system(f'mv test.log {results_dir}')
    os.system(f'mv test_dc.log {results_dir}')

    # run python script and save the output in a file, append if exists
    os.system(f'python3 discovery.py {results_dir}/test.log >> results/{test_name}_{now}_dr.txt')
    os.system(f'python3 energest-stats.py {results_dir}/test.log >> results/{test_name}_{now}_dc.txt')
