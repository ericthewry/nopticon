#! /usr/bin/python3

"""
Compute the uppermost cluster 
"""

from kmodes.kprototypes import KPrototypes
from sklearn.cluster import DBSCAN, KMeans, AgglomerativeClustering
import numpy as np
import matplotlib.pyplot as plt
from pprint import PrettyPrinter
import json
import sys
from argparse import ArgumentParser

def main():
    args = ArgumentParser(description='Compute the uppermost cluster of newline-separated list of inferences. Compute a matrix comparing the size of inferred properties')
    args.add_argument('-p', '--precision', dest="precision", action="store", default=8,
                      help="the rounding precision [default = 8, i.e. 10^-8 seconds]")
    args.add_argument('-t', '--threshold', dest="threshold", action="store", default=0.0,
                      help="only cluster points above the threshold")
    args.add_argument('-eq', '--equiv-classes', dest='equiv_classes', action="store_true", default=False,
                      help='use name-based equivalence classes')

    settings = args.parse_args()
 
    summaries = []
    for line in sys.stdin.readlines():
        try:
            if len(line) > 1:
                summaries.append(json.loads(line))
        except:
            print("tried to read: ", len(line))
            return

    all_prop = None
    inferences_per_summary = [set() for _ in summaries]
    for i, summary in enumerate(summaries):
        prop = {}
        ranks = []
        assert "reach-summary" in summary
        for reach in summary["reach-summary"]:
            assert "flow" in reach
            assert "edges" in reach
            flow = reach["flow"]
            for edge in reach["edges"]:
                assert "source" in edge
                assert "target" in edge
                assert "rank-0" in edge
                rank = round(edge["rank-0"], int(settings.precision))
                if  rank > float(settings.threshold):
                    prop[(reach["flow"],edge["source"],edge["target"])] = len(ranks)
                    if settings.equiv_classes:
                        ranks.append([rank, edge["source"][0], edge["target"][0] ])
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
            kranks = [ranks[idx][0]
                      for p,idx in prop.items()
                      if clust[idx] == k]
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

    print("Inferred", len(all_prop), "properties from", len(summaries), "summaries")
    for p in all_prop:
        print(" ".join(p))

    print()
    print("The following were unexpected")
    for f, s, t in all_prop:
        if t[0] != 'l':
            print(f,s,t)

    print("Precision: ", len([t for _,_,t in all_prop if t[0] == 'l'])/len(all_prop))
    print("Recall: ", len([t for _,_,t in all_prop if t[0] == 'l'])/152)

    pp = PrettyPrinter(width=80)
    comparisons = [[(0,0,0) for _ in inferences_per_summary]
                   for i in inferences_per_summary]
    
    for i, inf_i in enumerate(inferences_per_summary):
        for j, inf_j in enumerate(inferences_per_summary):
            if j <= i:
                continue
            comparisons[i][j] = (len([" ".join(e[0:3]) for e in inf_i.difference(inf_j)]),
                                 len(inf_i.intersection(inf_j)) ,
                                 len([" ".join(e[0:3]) for e in inf_j.difference(inf_i)]))
            comparisons[j][i] = tuple(reversed(comparisons[i][j]))

    for x in comparisons:
        print(*x, sep=" ")
    print("done")
    
if __name__ == "__main__":
    main()
