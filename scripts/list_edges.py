#!/usr/bin/python3

"""
List edges contained in a network summary
"""

from argparse import ArgumentParser
import ipaddress
import json

def main():
    # Parse arguments
    arg_parser = ArgumentParser(description='List edges contained in a network summary')
    arg_parser.add_argument('-summary', dest='summary_path', action='store',
            required=True, help='Path to summary JSON file')
    settings = arg_parser.parse_args()

    with open(settings.summary_path, 'r') as sf:
        summary_raw = sf.read()

    summary_json = json.loads(summary_raw)
    for key in sorted(summary_json.keys()):
        try:
            network = ipaddress.ip_network(key)
            print(network)
            for edge in summary_json[key]:
                print('\t%s->%s %f' % (edge['source'], edge['target'], edge['rank-0']))
        except:
            pass
#    for item in summary_json['flows']:
#        print(item)

if __name__ == '__main__':
    main()
