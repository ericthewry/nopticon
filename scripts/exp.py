#! /usr/bin/python

"""
This script is the central end-to-end experiment driver that evaluates nopticon's performance for SIGCOMM
"""

from kmodes.kprototypes import KPrototypes
from sklearn.cluster import DBSCAN, AgglomerativeClustering

from argparse import ArgumentParser
import nopticon
import implied_properties
import equivalence as eq
import numpy as np

def cluster(summ, agg_classes = None):
    """
    Clusters summary info using DBSCAN if agg_classes is provided it uses K-Prototypes
    """
    all_prop = None
    prop = {}
    ranks = []
    for flow, edge in summ.get_flowedges():
        rank = round(summ.get_edge_rank(flow, edge), 9)
        if rank < 0.5:
            continue
        policy = nopticon.ReachabilityPolicy({'flow' : flow,
                                              'source' : edge[0],
                                              'target' : edge[1]})
        prop[policy] = len(ranks)
        if agg_classes is not None:
            ranks.append([rank, agg_classes[edge[1]], agg_classes[edge[1]]])
        else:
            ranks.append([rank])
        
    if agg_classes is not None:
        kproto = KPrototypes(n_clusters=2, init='cao')
        clust = kproto.fit_predict(np.matrix(ranks).A, categorical=[1,2])
    else:
        agg = DBSCAN()#AgglomerativeClustering(n_clusters=2, linkage="ward")
        clust = agg.fit(ranks).labels_

    means = {}
    high = None
    for k in set(clust):
        kranks = [ranks[idx][0] for idx in prop.values() if clust[idx] == k]
        means[k] = sum(kranks)/len(kranks)
        if high is None or means[k] > means[high]:
            high = k
            
    for p,idx in prop.items():
        if clust[idx] == high:
            summ.mark_cluster_accepted(p.flow(), p.edge())
    return


def equivalence(summ, threshold):
    gNECs = eq.compute_general_NECs(threshold, summ)
    get_node_eqc_id = {}
    for idx,cls in enumerate(sorted(gNECs, key=lambda x: x[0])):
        for node in cls:
            get_node_eqc_id[node] = idx

    return get_node_eqc_id        

    
def threshold(summ, t):
    """
    Thresholds summ >= t
    """
    for f,e in summ.get_flowedges():
        if summ.get_edge_rank(f,e) > t:
            summ.mark_above_threshold(t,f,e)
    
def evaluate(summ, policies, cluster, implied, threshold):
    """
    Checks agreement between summ and policies, according to which experiments were run
    returns a tuple of Precision, Recall, Accuracy
    """
    
    true_positive_count = 0
    true_negative_count = 0
    false_positive_count = 0
    false_negative_count = 0
    for flow, edge in summ.get_flowedges():
        pol = nopticon.ReachabilityPolicy({'flow' : flow,
                                          'source' : edge[0],
                                          'target': edge[1]})
        if summ.is_insight(flow = flow,
                           edge = edge,
                           cluster = cluster,
                           threshold = threshold,
                           implied = implied):
            if pol in policies:
                true_positive_count += 1
            else:
                false_positive_count += 1
        else:
            if pol in policies:
                false_negative_count += 1
            else:
                true_negative_count += 1

    
    prec = true_positive_count/(true_positive_count + false_positive_count)
    rec  = true_positive_count/(true_positive_count + false_negative_count)
    acc  = (true_positive_count + true_negative_count) / (true_positive_count + true_negative_count + false_negative_count + false_positive_count)
    f1score = 2.0*(prec*rec)/(prec+rec)
    
    return (prec, rec, acc, f1score) 

def exp_quality_str(summ, policies, datafile, naive_cluster, agg_cluster, implied, threshold):
    precision,recall,accuracy,f1score = evaluate(summ, policies,
                                                 implied = implied,
                                                 threshold = threshold,
                                                 cluster = naive_cluster or agg_cluster)
    return "{0},{1},{2},{3},{4},{5},{6},{7},{8}\n".format(datafile,
                                                          agg_cluster,
                                                          naive_cluster,
                                                          implied,
                                                          threshold,
                                                          precision,
                                                          recall,
                                                          accuracy,
                                                          f1score)


def all_pairs(settings):
    if settings.completionist:
        return [(n, a, i, t)
                for n in set([settings.do_naive_clustering, False])
                for a in set([settings.do_agg_clustering, False])
                for i in set([settings.remove_implied_properties, False])
                for t in set([settings.threshold, None])]
    else:
        return [(settings.do_naive_clustering,
                settings.do_agg_clustering,
                settings.remove_implied_properties,
                settings.threshold)]
    
def main():
    parser = ArgumentParser(description="Run experiments on nopticon data for SIGCOMM")
    parser.add_argument("simulations",
                        help="A CSV file where the first column is a simulation dataset, the second column is the topology file for the simulation, and the third column is the policies file for the simulation")
    parser.add_argument("-c", "--do-naive-clustering", dest="do_naive_clustering", action="store_true",
                        default=False, help="setting this flag performs naive clustering")
    parser.add_argument("-C", "--do-agg-clustering", dest="do_agg_clustering", action="store_true",
                        default=False, help="setting this flag overrides the naive clustering flag and performes equivalence class based aggregation clustering")
    parser.add_argument("-I", "--remove-implied-properties", dest="remove_implied_properties", action="store_true",
                        default=False, help="Setting this flag executes naive clustering")
    parser.add_argument("-t", "--threshold", type=float, default=None, help="Providing this flag runs the experiments after discarding values below t")
    parser.add_argument("-p", "--completionist", action="store_true", default = False, help="setting this flag tests all combinations underneath the provided combination.")
    parser.add_argument("-o", "--outfile", dest="outfile", default=None,
                        help="The output csv file")


    settings = parser.parse_args()
    
    simulations = []
    with open(settings.simulations, 'r') as sim_fp:
        simulations = sim_fp.readlines()

    output = "simulation,agg_clustering,naive_clustering,reduction,threshold,precision,recall,accuracy,f1score\n"
        
    for sim in simulations:
        datafile, topofile, polfile = sim.split(',')
        fwd_summary, topo, policies = (None, None, None)
        with open(datafile, 'r') as data_fp:
            fwd_summary = nopticon.ReachSummary(data_fp.read(),9)
        with open(topofile, 'r') as topo_fp:
            topo = implied_properties.Topo(topo_fp.read())
        with open(polfile, 'r') as pol_fp:
            policies = nopticon.parse_policies(pol_fp.read())
        for idx, policy in enumerate(policies):
            if policy.isType(nopticon.PolicyType.PATH_PREFERENCE):
                policies[idx] = policy.toReachibilityPolicy()

        if settings.threshold is not None:
            # threshold
            threshold(fwd_summary, settings.threshold)

        if settings.do_agg_clustering:
            # do aggregated clustering
            cluster(fwd_summary, equivalence(fwd_summary, settings.threshold))
        elif settings.do_naive_clustering:
            # do naive clustering
            cluster(fwd_summary)

        if settings.remove_implied_properties:
            # remove implied properties
            implied_properties.mark_implied_properties(fwd_summary, topo, settings.threshold)
            
        # evaluate every result for the collected data
        for naive, agg, imp, thresh in all_pairs(settings):
            output += exp_quality_str(fwd_summary, policies, datafile, naive, agg, imp, thresh)
        
    if settings.outfile is None:
        print(output)
    else:
        with open(settings.outfile, 'w') as out_fp:
            # print to outfile
            out_fp.write(output)
    return
        
if __name__ == "__main__":
    main()

