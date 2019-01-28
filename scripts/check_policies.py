#!/usr/bin/python3

"""
Check whether all intents appear in a network summary
"""

from argparse import ArgumentParser
import ipaddress
import json
import math
import nopticon

def get_edge_rank(summary, flow, edge):
    rank = summary.get_edge_rank(flow, edge)
    if (rank is None):
        return (0, math.inf)
    edges = summary.get_edges(flow)
    ranks = sorted(set([details['rank-0'] for details in edges.values()]), 
            reverse=True)
    flow_rank = ranks.index(rank) + 1
    flow_percentile = flow_rank/len(ranks) * 100
    return (rank, flow_rank, flow_percentile)


def check_reachability(policy, summary):
    return get_edge_rank(summary, policy._flow, policy.edge())

def main():
    # Parse arguments
    arg_parser = ArgumentParser(description='Check whether intents appear in a network summary')
    arg_parser.add_argument('-summary', dest='summary_path', action='store',
            required=True, help='Path to summary JSON file')
    arg_parser.add_argument('-policies', dest='policies_path', action='store',
            required=True, help='Path to policies JSON file')
    arg_parser.add_argument('-extras', dest='extras', action='store_true',
            help='Output edges that do not correspond to any policies')
    settings = arg_parser.parse_args()

    # Load summary
    with open(settings.summary_path, 'r') as sf:
        summary_json = sf.read()
    summary = nopticon.ReachSummary(summary_json)

    # Load policies
    with open(settings.policies_path, 'r') as pf:
        policies_json = pf.read()
    policies = nopticon.parse_policies(policies_json)

    # Coerce path preference policies to reachability policy
    for idx, policy in enumerate(policies):
        if policy.isType(nopticon.PolicyType.PATH_PREFERENCE):
            policies[idx] = nopticon.ReachabilityPolicy({'flow' : policy._flow,
                    'source' : policy._paths[0][0],
                    'target' : policy._paths[0][-1]})

    # Check policies
    for policy in policies:
        if policy.isType(nopticon.PolicyType.REACHABILITY):
            reach_result = check_reachability(policy, summary)
            print('Policy %s %f %d %f' % (policy, reach_result[0], 
                reach_result[1], reach_result[2]))

    # Check for extra edges
    if (settings.extras):
        # Get all edges in policies
        policy_edges = {}
        for policy in policies:
            if policy.isType(nopticon.PolicyType.REACHABILITY):
                if policy._flow not in policy_edges:
                    policy_edges[policy._flow] = []
                policy_edges[policy._flow].append(policy.edge())

        # Identify extra edges
        for flow in summary.get_flows():
            for edge in summary.get_edges(flow):
                if edge not in policy_edges[flow]:
                    rank_result = get_edge_rank(summary, flow, edge)
                    print('Extra %s %s->%s %f %d %f' % (flow, edge[0], edge[1], 
                        rank_result[0], rank_result[1], rank_result[2]))

if __name__ == '__main__':
    main()
