#!/usr/bin/env python3

import argparse
import copy
import ipaddress
import yaml

BASE_OFFSET = 184549376 # 11.0.0.0

def main(args):

    with open(args.config_filename, 'r') as f:
        config = yaml.load(f, Loader=yaml.Loader)

    assert(len(config['hosts']) == 1)

    host = config['hosts'][0]
    del config['hosts'][0]

    base_addr = ipaddress.IPv4Address(BASE_OFFSET)

    for idx in range(1, host["quantity"] + 1):
        new_host = copy.deepcopy(host)
        new_host["id"] = host["id"] + str(idx)
        new_host["iphint"] = str(base_addr + idx)
        del new_host["quantity"]
        config['hosts'].append(new_host)

    with open(args.out_filename, 'w') as f:
        f.write(yaml.dump(config, Dumper=yaml.Dumper))

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("config_filename")
    parser.add_argument("out_filename")
    return parser.parse_args()

if __name__ == "__main__":
    main(parse_args())
