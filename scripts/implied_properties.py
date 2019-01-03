#! /usr/bin/python3

import nopticon
from argparse import ArgumentParser

class Topo:
    def __init__(self, topo_str):
        self._links = {}
        for line in topo_str.split('\n'):
            words = line.split(' ')
            if words[0] == "link":
                source = words[1].split(':')[0]
                target = words[2].split(':')[0]
                key = min(source, target)
                val = max(source, target)
                if key in self.links:
                    self._links[key].add(val)
                else:
                    self._links[key] = set(val)

    def _normalize(self, src, tgt):
        return (min(src,tgt), max(src,tgt))
                    
    def link_exists(self, source, target):
        key, val = self.normalize(src,tgt)
        return key in self.links and val in self.links[key]

class RestrictedGraph:
    def __init__(reach, topo, threshold):
        self._reach = reach
        self._topo = topo
        self._threshold = threshold
        
    def separate(self, flow, source, target):
        separator = set()
        edges = self._reach.get_edges(flow)
        rank = lambda e: reach.get_edge_rank(flow,e)
        # inferred_edges = {e : d for e : d in edges.items()
        #                   if rank(e) >= self.threshold}
        physical_edges = {e : d for e : d in edges.items()
                          if self._topo.link_exists(*e)}

        ## Compute _Close_ Separator Set in physical_edges
        # get successors S of source
        successors = [tgt for (src, tgt) in physical_edges.keys()
                      if src == source]
        # Compute Set of Nodes R that reach target, (not including those nodes in S)
        reach = set([ target ])
        separator = set()
        old_size = 0
        while len(reach) > old_size:
            old_size = len(reach)
            for (src, tgt) in physical_edges.keys():
                if tgt in reach:
                    assert(tgt not in successors)
                    if src in successors:
                        separator.add(src)
                    else:
                        reach.add(src)
            
            size = len(reach)
        
        return separator
        
def mark_implied_properties(reach, topo, threshold):
    g = RestrictedGraph(reach, topo, threshold)
    for flow in reach.get_flows():
        for (s,t) in reach.get_edges(flow):
            if topo.link_exists(s,t) or \
               reach.get_edge_rank(flow, (s,t)) < threshold:
                continue
            else:
                separator = g.separate(f, s, t) # compute an (s,t) separator C in G
                for v in separator:
                    reach.mark_edge_implied_by(f, (s,t), (c,t))

    
def main():
    parser = ArgumentParser(description="Remove Implied Properties")
    parser.add_argument("summary", help="The file path to a reachability summary")
    parser.add_argument("topo", help="The Topology file for the network from which the `summary` was collected")
    parser.add_argument("-t", "--threshold", default=50, type=int,
                        help="Threshold (as a percentage); ranks below the threshold are discarded; must be between 0 and 100")

    settings = parse.parse_args()

    if settings.threshold > 100 or settings.threshold < 0:
        print("ERROR: Value supplied to --threshold must be between 0 and 100. You supplied", settings.threshold)
        return 1
    else:
        threshold = float(settings.threshold) / 100.0


    reach_str = None
    with open(settings.summary) as reach_fp:
        reach_str = reach_fp.read()
        
    if reach_str is None:
        print("ERROR: Could not read from", settings.summary)
        return 1

    reach_summ = ReachSummary(reach_str, 2)
    
    topo_str = None
    with open(settings.topo) as topo_fp:
        topo_str = topo_fp.read()

    topo = Topo(topo_str)
    
    mark_implied_properties(reach_summ, topo, threshold)

    
    

if __name__ == "__main__":
    main()
