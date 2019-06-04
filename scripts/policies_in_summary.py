#! /usr/bin/python3

"""
Draw a graph that separates the reachability policies from the forwarding summary
"""

import nopticon
import json
import matplotlib.pyplot as plt
from argparse import ArgumentParser

def draw_policy_sep(summary, policies, outfile):
    datapoints = []
    for flow in summary.get_flows():
        for edge in summary.get_edges(flow):
            rank = summary.get_edge_rank(flow, edge)
            if rank > 0.9 :
                
                policy = nopticon.ReachabilityPolicy({'flow' : flow, 'source' : edge[0], 'target' : edge [1]})
                datapoints.append((rank, policy in policies ))

    fig = plt.figure()
    # ax = plt.subplot()
    # make histograms
    plt.hist([[x for x,p in datapoints if p], [x for x,p in datapoints if not p]],bins=20, stacked=True)
    
    fig.savefig(outfile)
    
    return 
    


def main():
    parser = ArgumentParser(description='draw a graph highlighting policies in the forwarding summary')
    parser.add_argument('-s', '--summary', action='store', help="A filepath to the Forwarding summary")
    parser.add_argument('-p', '--policies', action='store', help="A filepath to the policies")
    parser.add_argument('-o', '--outfile', action='store', help="The file to output the graph")

    settings = parser.parse_args()

    # Load Policies
    with open(settings.summary, 'r') as sf:
        sf_str = sf.read()
        summary = nopticon.ReachSummary(sf_str, 9)
        
    # Load Policies
    with open(settings.policies, 'r') as pf:
        policies_json = pf.read()
    policies = nopticon.parse_policies(policies_json)

    for idx, policy in enumerate(policies):
        if policy.isType(nopticon.PolicyType.PATH_PREFERENCE):
            policies[idx] = policy.toReachabilityPolicy()

    draw_policy_sep(summary, policies, settings.outfile)


if __name__ == "__main__":
    main()
