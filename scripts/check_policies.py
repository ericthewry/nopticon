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

def check_path_preference(policy, summary):
    paths = policy.get_paths()
    ranks = []
    for i in range(0, len(paths)-1):
        ranks.append(summary.get_comparison_rank(policy.get_flow(), paths[i], 
            paths[i+1]))
    return ranks

def main():
    # Parse arguments
    arg_parser = ArgumentParser(description='Check whether intents appear in a network summary')
    arg_parser.add_argument('-s', '--summary', dest='summary_path', 
            action='store', required=True, help='Path to summary JSON file')
    arg_parser.add_argument('-p', '--policies', dest='policies_path', 
            action='store', required=True, help='Path to policies JSON file')
    arg_parser.add_argument('-e', '--extras', dest='extras', 
            action='store_true', 
            help='Output edges that do not correspond to any policies')
    arg_parser.add_argument('-c', '--coerce', dest='coerce', 
            action='store_true',
            help='Coerce path-preference policies to reachability policies')
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

    # Coerce path preference policies to reachability policy, if requested
    if (settings.coerce):
        for idx, policy in enumerate(policies):
            if policy.isType(nopticon.PolicyType.PATH_PREFERENCE):
                policies[idx] = policy.toReachabilityPolicy()

    # Check policies
    for policy in policies:
        if policy.isType(nopticon.PolicyType.REACHABILITY):
            reach_result = check_reachability(policy, reach_summary)
            print('Policy %s %f %d %f' % (policy, reach_result[0], 
                reach_result[1], reach_result[2]))
        elif policy.isType(nopticon.PolicyType.PATH_PREFERENCE):
            pref_result = check_path_preference(policy, path_summary)
            print('Policy %s %s' % (policy, ' '.join(map(str, pref_result))))

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
        for flow in reach_summary.get_flows():
            for edge in reach_summary.get_edges(flow):
                if edge not in policy_edges[flow]:
                    rank_result = get_edge_rank(reach_summary, flow, edge)
                    print('Extra %s %s->%s %f %d %f' % (flow, edge[0], edge[1], 
                        rank_result[0], rank_result[1], rank_result[2]))

if __name__ == '__main__':
    main()
