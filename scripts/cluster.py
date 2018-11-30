#! /usr/bin/python3

"""
Compute the uppermost cluster 
"""

from kmodes.kprototypes import KPrototypes
from sklearn.cluster import DBSCAN, KMeans, AgglomerativeClustering
import numpy as np
import matplotlib.pyplot as plt
from pprint import PrettyPrinter
import nopticon
import sys
from argparse import ArgumentParser

def infer_reachability(summaries, settings):
    all_prop = None
    inferences_per_summary = [set() for _ in summaries]
    for i, summary in enumerate(summaries):
        prop = {}
        ranks = []
        for flow in summary.get_flows():
            for edge in summary.get_edges(flow):
                rank = round(summary.get_edge_rank(flow, edge), 
                        settings.precision)
                if rank > float(settings.threshold):
                    policy = nopticon.ReachabilityPolicy({'flow' : flow,
                        'source' : edge[0], 'target' : edge[1]})
                    prop[policy] = len(ranks)
                    if settings.equiv_classes:
                        # TODO: genericize class
                        ranks.append([rank, edge[0][0], edge[1][0]])
                    else:
                        ranks.append([rank])

        if settings.equiv_classes:
            kproto = KPrototypes(n_clusters=2, init='cao')
            clust = kproto.fit_predict(np.matrix(ranks).A, categorical=[1,2])
        else:
            agg = AgglomerativeClustering(n_clusters=2, linkage="ward")
            clust = agg.fit(ranks).labels_

        fig = plt.figure()
        ax = plt.subplot()

        colors = ['green', 'red', 'blue', 'purple', 'cyan', 'orange']

        means = {}
        high = None
        for k in set(clust):
            kranks = [ranks[idx][0] for idx in prop.values() if clust[idx] == k]
            means[k] = sum(kranks)/len(kranks)
            if high is None or means[k] > means[high]:
                high = k

        props_to_isect = set([p for p,idx in prop.items() if clust[idx] == high])
        inferences_per_summary[i] = props_to_isect
        if all_prop is None:
            all_prop = props_to_isect
        else:
            all_prop = all_prop.intersection(props_to_isect)

        # exp_colors = ['green' for _ in ranks]
        # for (f,s,t), idx in prop.items():
        #     if t[0] != 'l':
        #         exp_colors[idx] = 'red'
        
        #         clust_colors = [colors[l] if l >= 0 else "black" for l in clust.labels_]
        
        #         for k in range(0,2):
        #             ax.scatter(clust.labels_, [r for rs in ranks for r in rs],
        #                        c=exp_colors)
                    
        # fig.savefig("cluster.png")

    return (all_prop, inferences_per_summary)


def main():
    args = ArgumentParser(description='Compute the uppermost cluster of newline-separated list of inferences. Compute a matrix comparing the size of inferred properties')
    args.add_argument('-s', '--summary', dest='summary_path', action='store',
            required=True, help='Path to summary JSON file')
    args.add_argument('-p', '--policies', dest='policies_path', action='store',
            required=True, help='Path to policies JSON file')
    args.add_argument('-r', '--precision', dest="precision", action="store", 
            default=8, type=int,
            help="the rounding precision [default = 8, i.e. 10^-8 seconds]")
    args.add_argument('-t', '--threshold', dest="threshold", action="store", 
            default=0.0, type=float,
            help="only cluster points above the threshold (default=0)")
    args.add_argument('-e', '--equiv-classes', dest='equiv_classes', 
            action="store_true", default=False,
            help='use name-based equivalence classes')
    settings = args.parse_args()

    # Load summaries
    summaries = []
    with open(settings.summary_path, 'r') as sf:
        for summary_json in sf:
            summaries.append(nopticon.ReachSummary(summary_json))

    # Load policies
    with open(settings.policies_path, 'r') as pf:
        policies_json = pf.read()
    policies = nopticon.parse_policies(policies_json)
    
    # Coerce path preference policies to reachability policy
    for idx, policy in enumerate(policies):
        if policy.isType(nopticon.PolicyType.PATH_PREFERENCE):
            policies[idx] = policy.toReachabilityPolicy()

    # Infer rechability properties
    all_prop, inferences_per_summary = infer_reachability(summaries, settings)
            
    print("Inferred %d policies from %d summaries" % 
            (len(all_prop), len(summaries)))

    correct_policies = 0
    for p in all_prop:
        valid = p in policies
        if (valid):
            correct_policies += 1
        print("%s %s" % (p, valid))

    print("Precision: %f" % (correct_policies/len(all_prop)))
    print("Recall: %f" % (correct_policies/len(policies)))

#    pp = PrettyPrinter(width=80)
#    comparisons = [[(0,0,0) for _ in inferences_per_summary]
#                   for i in inferences_per_summary]
#    
#    for i, inf_i in enumerate(inferences_per_summary):
#        for j, inf_j in enumerate(inferences_per_summary):
#            if j <= i:
#                continue
#            comparisons[i][j] = (len([" ".join(e[0:3]) for e in inf_i.difference(inf_j)]),
#                                 len(inf_i.intersection(inf_j)) ,
#                                 len([" ".join(e[0:3]) for e in inf_j.difference(inf_i)]))
#            comparisons[j][i] = tuple(reversed(comparisons[i][j]))
#
#    for x in comparisons:
#        print(*x, sep=" ")
#    print("done")
    
if __name__ == "__main__":
    main()
