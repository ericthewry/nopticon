#!/usr/bin/python3

"""
Compute reachability rank for edges based on the number of failure scenarios in 
which the edge's rank exceeds a threshold.
"""

from argparse import ArgumentParser
import ipaddress
import json
import nopticon

def process_summary(summary, counts, threshold):
    # Process every flow in summary
    for flow in summary.get_flows():
        # Handle new flow
        if flow not in counts:
            counts[flow] = {}

        # Process every edge for flow in summary
        for edge in summary.get_edges(flow):
            # Handle new edge
            if edge not in counts[flow]:
                counts[flow][edge] = 0

#            print("%s %s %f" % (flow, edge, summary.get_edge_rank(flow, edge)))

            # Count edge as present if edge rank is above threshold
            if summary.get_edge_rank(flow, edge) >= threshold:
                counts[flow][edge] += 1

def main():
    # Parse arguments
    arg_parser = ArgumentParser(description="Compute reachability rank for "
            + "edges based on the number of failure scenarios in which the "
            + "edge's rank exceeds a threshold")
    arg_parser.add_argument('-summaries', dest='summaries_path', action='store',
            required=True, help='Path to summaries JSON file')
    arg_parser.add_argument('-threshold', dest='rank_threshold', action='store',
            default=0, type=float, help='Minimimum required rank of edge under '
            + 'a given failure scenario to be considered present (default=0)')
    settings = arg_parser.parse_args()

    # Process per-failure-scenario summaries
    scenarios = 0
    counts = {} 
    with open(settings.summaries_path, 'r') as sf:
        for summary_json in sf.readlines():
#            print("="*80)
            summary = nopticon.ReachSummary(summary_json)
            process_summary(summary, counts, settings.rank_threshold)
            scenarios += 1

    # Compute edge rank
    flows = []
    for flow in counts:
        edges = []
        for edge in counts[flow]:
            edges.append({'source' : edge[0], 'target' : edge[1], 
                'rank-0' : counts[flow][edge] / scenarios})
        flows.append({'flow' : str(flow), 'edges' : edges})

    print(json.dumps({'reach-summary' : flows}))

if __name__ == '__main__':
    main()
