#!/usr/bin/env python3

from __future__ import division

import re
import sys
import os.path
import argparse
from datetime import datetime
import math

nodes = []

def parse_file(log_file, testbed=False):
    # Print some basic information for the user
    print(f"Logfile: {log_file}")
    print(f"{'Cooja simulation' if not testbed else 'Testbed experiment'}\n")

    # Regular expressions
    if testbed:
        # Regex for testbed experiments
        testbed_record_pattern = r"\[(?P<time>.{23})\] INFO:firefly\.(?P<self_id>\d+): \d+\.firefly < b"
        regex_num_nbr = re.compile(r"{}'.*Epoch (?P<epoch_num>\d+) finished Num NBR (?P<num_nbr>\d+)".format(testbed_record_pattern))
    else:
        # Regular expressions for COOJA
        record_pattern = r"(?P<time>[\w:.]+)\s+ID:(?P<self_id>\d+)\s+"
        regex_num_nbr = re.compile(r"{}.*Epoch (?P<epoch_num>\d+) finished Num NBR (?P<num_nbr>\d+)".format(record_pattern))

    data = {}

    epochs = 0

    time = None

    # Parse log file and add data to CSV files
    with open(log_file, 'r') as f:
        for line in f:
            
            m = regex_num_nbr.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    if time is None:
                        time = ts
                    ts = ts.timestamp()
                else:
                    ts = d["time"]

                d['self_id'] = int(d['self_id'])
                d['epoch_num'] = int(d['epoch_num'])
                d['num_nbr'] = int(d['num_nbr'])

                if data.get(d['self_id']) is None:
                    data[d['self_id']] = {
                        'self_id': d['self_id'],
                        'epoch_num': 0,
                        'num_nbr': [],
                        'total_nbr': 0,
                    }

                data[d['self_id']]['epoch_num'] = d['epoch_num']

                data[d['self_id']]['num_nbr'].append(d['num_nbr'])
                data[d['self_id']]['total_nbr'] += d['num_nbr']

    if testbed: 
        print(f"Time: {time}")

    # Analyse and print the data
    dr_lst = []

    # nodes
    # epochs = max([v['epoch_num'] for v in data.values()]) + 1 # zero based
    ordered_keys = sorted(data.keys())
    available_nbrs = len(ordered_keys) - 1
    print(f"Nodes: {len(ordered_keys)} {ordered_keys}")
    for nid in ordered_keys:
        v = data[nid]

        theoretical_max_nbrs = (available_nbrs * (v['epoch_num']+1))
        dr = (v['total_nbr'] / theoretical_max_nbrs)
        print("Node {}: {} ({}) out of {} on {} epochs = {:.2%}".format(nid, v['total_nbr'], sum(v['num_nbr']), theoretical_max_nbrs, v['epoch_num']+1, dr))

        dr_lst.append(dr*100)
        

    dc_mean = sum(dr_lst)/len(dr_lst)
    dc_min = min(dr_lst)
    dc_max = max(dr_lst)
    dc_std = math.sqrt(sum([(v - dc_mean)**2 for v in dr_lst]) / len(dr_lst))

    print("\n----- Discovery Rate Overall Statistics -----\n")
    print("Average Discovery Rate: {:.2f}%\nStandard Deviation: {:.2f}\n"
          "Minimum: {:.2f}%\nMaximum: {:.2f}%\n".format(dc_mean,
                                                        dc_std, dc_min,
                                                        dc_max))


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('logfile', action="store", type=str,
                        help="data collection logfile to be parsed and analyzed.")
    parser.add_argument('-t', '--testbed', action='store_true',
                        help="flag for testbed experiments")
    return parser.parse_args()


if __name__ == '__main__':

    args = parse_args()
    print(args)

    if not args.logfile:
        print("Log file needs to be specified as 1st positional argument.")
    if not os.path.exists(args.logfile):
        print("The logfile argument {} does not exist.".format(args.logfile))
        sys.exit(1)
    if not os.path.isfile(args.logfile):
        print("The logfile argument {} is not a file.".format(args.logfile))
        sys.exit(1)

    # Parse log file, create CSV files, and print some stats
    parse_file(args.logfile, testbed=args.testbed)

