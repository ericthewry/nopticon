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
import matplotlib.pyplot as plt

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
    return (datafile, agg_cluster, naive_cluster, implied,
            threshold, precision, recall, accuracy, f1score)


def all_pairs(settings):
    if settings.completionist:
        return [(n, a, i, t)
                for n in set([settings.do_naive_clustering, False])
                for a in set([settings.do_agg_clustering, False])
                for i in set([settings.remove_implied_properties, False])
                for t in set([settings.threshold])
                if n != a or (not n and not a)]
    else:
        return [(settings.do_naive_clustering,
                settings.do_agg_clustering,
                settings.remove_implied_properties,
                settings.threshold)]

def write_experiments(to_directory, from_data):
    naive_cluster = sorted([(prettify_name(df), prec, rec, acc, f1) for df, ag, na, imp, thrsh, prec, rec, acc, f1 in from_data
                            if na and not ag and not imp], key = lambda x: x[0])
    agg_cluster = sorted([(prettify_name(df), prec, rec, acc, f1) for df, ag, na, imp, thrsh, prec, rec, acc, f1 in from_data
                          if ag and not imp], key=lambda x: x[0])
    remove_consequences = sorted([(prettify_name(df), prec, rec, acc, f1) for df, ag, na, imp, thrsh, prec, rec, acc, f1 in from_data
                                  if imp and not ag and not na], key=lambda x: x[0])
    naive_remove = sorted([(prettify_name(df), prec, rec, acc, f1) for df, ag, na, imp, thrsh, prec, rec, acc, f1 in from_data
                           if na and imp and not ag], key=lambda x: x[0])
    agg_remove = sorted([(prettify_name(df), prec, rec, acc, f1) for df, ag, na, imp, thrsh, prec, rec, acc, f1 in from_data
                         if ag and imp], key = lambda x: x[0])
    
    plot( directory = to_directory,
          lines = [naive_cluster, agg_cluster, remove_consequences, naive_remove, agg_remove],
          names = ["NAI",         "AGG",       "REM",               "NAI_REM",    "AGG_REM"],
          views =   [(0,1),       (0,2),    (0,3),      (0,4)],
          y_axes =  ["precision", "recall", "accuracy", "F1-score"])

def prettify_name(name):
    outputname = ""
    if "fattree-4" in name:
        outputname += "F4"

    if "iterate" in name:
        outputname += "-I"
        
    if "16" in name:
        outputname += "-16"
    elif "32" in name:
        outputname += "-32"
    elif "64" in name:
        outputname += "-64"

    if "end" in name:
        outputname += "-E"
    elif "scenario" in name:
        outputname += "-S"

    return outputname
    

    
def plot(directory, lines, names, views, y_axes, x_axis = "simulation"):
    assert len(views) == len(y_axes)
    assert len(lines) == len(names)

    for idx, (x_idx,y_idx) in enumerate(views):
        plt.figure()
        y_axis_label = y_axes[idx]
        for l_idx, line in enumerate(lines):
            xs = [ p[x_idx] for p in line ]
            ys = [ p[y_idx] for p in line ]
            print(names[l_idx], y_axis_label)
            print(xs)
            print(ys)
            plt.plot(xs,ys, label = names[l_idx])
                     
        plt.ylabel(y_axis_label)
        plt.xlabel(x_axis)
        plt.ylim(0,1)
        plt.legend()
        plt.savefig("{}/{}-{}.pdf".format(directory, x_axis, y_axis_label))
        plt.close('all')
    
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
    parser.add_argument("-V", "--visualize", default=None, help="A directory into which charts are output")
    parser.add_argument("-o", "--outfile", dest="outfile", default=None,
                        help="The output csv file")


    settings = parser.parse_args()
    
    simulations = []
    with open(settings.simulations, 'r') as sim_fp:
        simulations = sim_fp.readlines()


    output = []
    for sim in simulations:
        datafile, topofile, polfile = sim.split(',')
        fwd_summary, topo, policies = (None, None, None)
        with open(datafile.strip(), 'r') as data_fp:
            fwd_summary = nopticon.ReachSummary(data_fp.read(),9)
        with open(topofile.strip(), 'r') as topo_fp:
            topo = implied_properties.Topo(topo_fp.read())
        with open(polfile.strip(), 'r') as pol_fp:
            policies = nopticon.parse_policies(pol_fp.read())
        for idx, policy in enumerate(policies):
            if policy.isType(nopticon.PolicyType.PATH_PREFERENCE):
                policies[idx] = policy.toReachibilityPolicy()


        # evaluate every result for the collected data
        for naive, agg, imp, thresh in all_pairs(settings):
            if thresh is not None:
                # threshold
                threshold(fwd_summary, settings.threshold)

            if agg:
                # do aggregated clustering
                cluster(fwd_summary, equivalence(fwd_summary, settings.threshold))
            elif naive:
                # do naive clustering
                cluster(fwd_summary)

            if imp:
                # remove implied properties
                implied_properties.mark_implied_properties(fwd_summary, topo, settings.threshold)
            
            output.append(exp_quality_str(fwd_summary, policies, datafile, naive, agg, imp, thresh))

            fwd_summary.clear()

    if settings.visualize is not None:
        write_experiments(to_directory=settings.visualize, from_data=output)

    output.append(tuple("simulation,agg_clustering,naive_clustering,reduction,threshold,precision,recall,accuracy,f1score".split(",")))

    output_str = '\n'.join(','.join([str(o) for o in output]))
    if settings.outfile is None:
        print(output_str)
    else:
        with open(settings.outfile, 'w+') as out_fp:
            # print to outfile
            out_fp.write(output_str)
    return
        
if __name__ == "__main__":
    main()

