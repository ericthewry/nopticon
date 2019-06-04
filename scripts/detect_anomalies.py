#! /usr/bin/python3

"""
Detects anomalies between an input summary and a stin stream of them.

Usage:

given a file named inference.json, which is the inference summary from
a different run of nopticon,

```
gobmpd | gobgp-analysis --reach-summary $SPAN --verbosity 8 $RDNS | detect_anomalies -t 0.75 -i inference.json
```
"""

import json
import math
import sys
from argparse import ArgumentParser


class AnomalyDetector:
    """
    Detect anomalies!
    """
    
    def __init__(self,inf_summ,threshold):
        """
        Given a ground truth inferred reachability summary from Nopticon, `inf_summ`
        and a `threshold` for rank reporting (i.e. if an observation in the 
        classification summary differs from `inf_summ` by more than `threshold`,
        then we will report it.
        """
        assert 0.0 <= threshold and threshold <= 1.0
        self.threshold = threshold
        self.inf_summ = {}
        for r in inf_summ["reach-summary"]:
            f = r["flow"]
            if f not in self.inf_summ:
                self.inf_summ[f] = {}
            for e in r["edges"]:
                src = e["source"]
                tgt = e["target"]
                rnk = e["rank-0"]
                if src not in self.inf_summ[f]:
                    self.inf_summ[f][src] = {tgt : rnk}
                else:
                    self.inf_summ[f][src][tgt] = rnk

    def check_and_report(self, cls_summ):
        """
        Checks whether the classification summary json `cls_summ` agrees with the
        inferred summary with the epsilon specified by the objects `threshold`.
        """
        for r in cls_summ["reach-summary"]:
            f = r["flow"]
            if f not in self.inf_summ:
                #print(r, "MISSING FLOW")
                continue
            for e in r["edges"]:
                src = e["source"]
                tgt = e["target"]
                rnk = e["rank-0"]
                if src not in self.inf_summ[f]:
                    # print(f,e, "MISSING FLOW, SRC")
                    continue
                elif tgt not in self.inf_summ[f][src]:
                    #print(f,e, "MISSING (FLOW,SRC,TGT)")
                    continue
                elif abs(self.inf_summ[f][src][tgt] - rnk) > self.threshold and self.inf_summ[f][src][tgt] > 0.9:
                    print(f, e, self.inf_summ[f][src][tgt])
                    continue

def main():
    arg_parser = ArgumentParser(description='Compute anomalies between summaries')
    arg_parser.add_argument('-i','--inf-summ', dest='inf_summ', action='store',
                            required=True, help='File Path to the inference summary')
    arg_parser.add_argument('-t', '--threshold', dest='threshold', action='store',
                            required=True, help='Report the properties that have difference greater than the threshold, where the threshold is between 0.0 and 1.0')

    settings = arg_parser.parse_args()
    inf_summ_json = {}
    with open(settings.inf_summ) as inf_fp:
        inf_summ_json = json.load(inf_fp)
    detector = AnomalyDetector(inf_summ_json, float(settings.threshold))
    for line in sys.stdin:
        detector.check_and_report(json.loads(line))


if __name__ == "__main__":
    main()
