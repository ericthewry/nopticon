#! /usr/bin/python3

from nopticon import ReachSummary, PolicyType, parse_policies
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
                if key in self._links:
                    self._links[key].add(val)
                else:
                    self._links[key] = set(val)

    def _normalize(self, src, tgt):
        return (min(src,tgt), max(src,tgt))
                    
    def link_exists(self, source, target):
        key, val = self._normalize(source,target)
        return key in self._links and val in self._links[key]

class RestrictedGraph:
    def __init__(self, reach, topo, threshold):
        self._reach = reach
        self._topo = topo
        self._threshold = threshold
        
    def separate(self, flow, source, target):
        separator = set()
        edges = self._reach.get_edges(flow)
        rank = lambda e: round(self._reach.get_edge_rank(flow,e),2)
        # inferred_edges = {e : d for e : d in edges.items()
        #                   if rank(e) >= self.threshold}
        physical_edges = {e : d for e, d in edges.items()
                          if self._topo.link_exists(*e)
                          if rank(e) > 0}
        if str(flow) == "3.0.0.0/24" and source == "leaf3_0" and target == "agg0_1":
            print("Separating", source, "and", target)

        ## Compute _Close_ Separator Set in physical_edges
        # get successors S of source
        successors = set([tgt for (src, tgt) in physical_edges.keys()
                          if src == source])
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

        if str(flow) == "3.0.0.0/24" and source == "leaf3_1" and target == "agg0_1":
            print(source, target, "separated by:", separator)
        return separator
        
def mark_implied_properties(reach, topo, threshold):
    g = RestrictedGraph(reach, topo, threshold)
    for flow in reach.get_flows():
        for (s,t) in reach.get_edges(flow):
            if topo.link_exists(s,t) or \
               reach.get_edge_rank(flow, (s,t)) < threshold:
                continue
            else:
                separator = g.separate(flow, s, t)
                for v in separator:
                    # if str(flow) == "3.0.0.0/24":
                    #     print((s,t), "==>", (v,t))
                    reach.mark_edge_implied_by(flow,
                                               premise=(s,t),
                                               conclusion=(v,t))

    
def main():
    parser = ArgumentParser(description="Remove Implied Properties")
    parser.add_argument("summary", help="The file path to a reachability summary")
    parser.add_argument("topo", help="The Topology file for the network from which the `summary` was collected")
    parser.add_argument("--include-implied", dest="include_implied", action="store_true",
                        help="Include the implied properties in the output")
    parser.add_argument("-t", "--threshold", default=50, type=int,
                        help="Threshold (as a percentage); ranks below the threshold are discarded; must be between 0 and 100")
    parser.add_argument("-p", "--policies-path", dest="policies_path", default=None,
                        help="List of expected policies for the `summary`")
    
    settings = parser.parse_args()

    # check threshold has a valid value
    if settings.threshold > 100 or settings.threshold < 0:
        print("ERROR: Value supplied to --threshold must be between 0 and 100. You supplied", settings.threshold)
        return 1
    else:
        threshold = float(settings.threshold) / 100.0


    if settings.policies_path is not None:
        # load policies
        with open(settings.policies_path, 'r') as pf:
            policies_json = pf.read()
        policies = nopticon.parse_policies(policies_json)
    
        # Coerce path preference policies to reachability policy
        for idx, policy in enumerate(policies):
            if policy.isType(nopticon.PolicyType.PATH_PREFERENCE):
                policies[idx] = policy.toReachabilityPolicy()
                
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
    
    props = reach_summ.to_policy_set(show_implied=settings.include_implied, threshold=threshold)

    if settings.policies_path is None:
        for p in props:
            print(p)
    else:
        correct_policies = 0
        for p in props:
            correct_policies += int(p in policies)

        print("Precision:", float(correct_policies/len(props)))
        print("Recall:", float(correct_policies/len(policies)))
    
if __name__ == "__main__":    
    main()
