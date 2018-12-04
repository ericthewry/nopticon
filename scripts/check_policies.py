#!/usr/bin/python3

"""
Check whether all intents appear in a network summary
"""

from argparse import ArgumentParser
import ipaddress
import json
import nopticon

def check_reachability(policy, summary):
    edge = (policy._source, policy._target)
    if edge not in summary.get_edges(policy._flow):
        return -1
    edge_details = summary.get_edges(policy._flow)[edge]
    return edge_details['rank-0']

def main():
    # Parse arguments
    arg_parser = ArgumentParser(description='List edges contained in a network summary')
    arg_parser.add_argument('-summary', dest='summary_path', action='store',
            required=True, help='Path to summary JSON file')
    arg_parser.add_argument('-policies', dest='policies_path', action='store',
            required=True, help='Path to policies JSON file')
    settings = arg_parser.parse_args()

    # Load summary
    with open(settings.summary_path, 'r') as sf:
        summary_json = sf.read()
    reach_summary = nopticon.ReachSummary(summary_json)
    path_summary = nopticon.PathPreferenceSummary(summary_json)

    # Load policies
    with open(settings.policies_path, 'r') as pf:
        policies_json = pf.read()
    policies = nopticon.parse_policies(policies_json)

    for policy in policies:
        if policy.isType(nopticon.PolicyType.REACHABILITY):
            print('%s %f' % (policy, check_reachability(policy, reach_summary)))
        elif policy.isType(nopticon.PolicyType.PATH_PREFERENCE):
            # Coerce path preference policy to reachability policy
            reach_policy = nopticon.ReachabilityPolicy({'flow' : policy._flow,
                    'source' : policy._paths[0][0],
                    'target' : policy._paths[0][-1]})
            print('%s (%s) %f' % (policy, reach_policy,
                    check_reachability(reach_policy, reach_summary)))

if __name__ == '__main__':
    main()
