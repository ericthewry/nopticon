#! /usr/bin/python3


####
#  
#  Compute Node Equivalence Classes for Reachability summaries
#
####

import nopticon
from argparse import ArgumentParser

def height(succ, node, memo):
    if node not in memo:
        if node in succ:
            memo[node] = 1 + max([height(succ, s, memo) for s in succ[node] ])
        else:
            memo[node] = 0
    return memo[node]

    
def compute_flow_NECs(edges):
    adj = { source: set([ target for src, target in edges
                          if src == source])
            for source,_ in edges }
    op_adj = { target : set([source for source, tgt in edges
                             if tgt == target])
               for _, target in edges }

    # Compute the join of the two key sets
    nodes = set(adj.keys()).union(set(op_adj.keys()))
    memo = {}
    op_memo = {}
    nec_list = { n: (height(adj, n, memo), height(op_adj, n, op_memo)) for n in nodes }
    return nec_list
    
    

def compute_general_NECs(threshold, reach):
    all_fNECs = {}
    for f in reach.get_flows():
        necs = compute_flow_NECs([edge for edge in reach.get_edges(f).keys()
                                  if reach.get_edge_rank(f,edge) >= threshold])
        # print(f,necs)
        # print()
        for n, eqc in necs.items():
            if n in all_fNECs:
                all_fNECs[n].append((f,eqc))
            else:
                all_fNECs[n] = [(f,eqc)]

    for n in all_fNECs.keys():
        all_fNECs[n] = sorted([eqc for _,eqc in all_fNECs[n]])

    gNECs = set()
    for n in all_fNECs.keys():
        for cls in gNECs:
            if n in cls: #if we've already placed n in an equivalence class
                continue
        cls = (n,)
        for m in all_fNECs.keys():
            if n != m:
                if all_fNECs[n] == all_fNECs[m]:
                    cls += (m,)
        gNECs.add(tuple(sorted(cls)))
        
    return gNECs
            
    


def main():
    parser = ArgumentParser(description = "Compute Node Equivalence Classes for a given Reachability Summary and rDNS")
    parser.add_argument("-t", "--threshold", default=50.0, type=int, required=False,
                        help="The minimum rank to consider out of 100, e.g. A value of 75 corresponds to a rank of 0.75 ")
    parser.add_argument("-s", "--sigfigs", default=2, type=int, required=False,
                        help="The number of sig figs for the rank values")
    parser.add_argument("summary", help="The filepath to the reachability summary in JSON form")
    settings = parser.parse_args()

    if settings.threshold > 100:
        print("Threshold must be between 0 and 100")
        return 1
    
    rs = None
    
    with open(settings.summary) as reach_fp:
        rs = nopticon.ReachSummary(reach_fp.read(), settings.sigfigs)
        
    gNECs = compute_general_NECs(float(float(settings.threshold)/100.0), rs)

    # print each equivalence class in
    for eqC in sorted(gNECs, key = lambda x: x[0]): 
        print(*eqC)
        
if __name__ == "__main__":
    main()
