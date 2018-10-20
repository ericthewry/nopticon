#! /usr/bin/python

import json
import sys
from argparse import ArgumentParser

SRC = 2
TGT = 3
IP_PREF = 1
TIME = 0

class Update:
    """
    Internal representation of an individual BGP Update or CSV Update
    """

    def __init__(self, update_str):
        """
        Parses a Logical Rule Update from a csv string

        Expected Format is
        +<timestamp>,<subnet_prefix>,<source_name>,<next_hop_name>
        or 
        -<timestamp>,<subnet_prefix>,<source_name>
        """
        self.is_widthdraw = update_str[0] == "-"        
        fields = update_str[1:].strip().split(",")
        assert(len(fields) == 4)
        self.source = fields[SRC]
        self.target = fields[TGT]
        self.ip_prefix = fields[IP_PREF]
        self.timestamp = fields[TIME]
        
    
    def to_BGP_string(self, rDNS):
        """
        Returns a string representing the rule update in GoBGP format
        """
        print(str(rDNS))
        if self.is_widthdraw:
            bgp = {"Header": {"Type" : 0},
                   "PeerHeader" : {"PeerBGPID" : rDNS[self.source][self.target]["source"],
                                   "Timestamp" : int(self.timestamp)},
                   "Body" : {
                       "BGPUpdate" : {
                           "Body" : {
                               "PathAttributes" : [],
                               "NLRI" : [],
                               "WithdrawnRoutes" : [{"prefix" : self.ip_prefix}]
                           }
                       }
                   }
            }
        else:
            print(rDNS, str([self.source]), str([self.target]))
            bgp = {"Header": {"Type" : 0},
                   "PeerHeader" : {"PeerBGPID" : rDNS[self.source][self.target]["source"],
                                   "Timestamp" : int(self.timestamp)},
                   "Body" : {
                       "BGPUpdate" : {
                           "Body" : {
                               "PathAttributes" : [{"type" : 3,
                                                    "nexthop" : rDNS[self.source][self.target]["target"]}],
                               "NLRI" : [{"prefix" :  self.ip_prefix }],
                               "WithdrawnRoutes" : []
                           }
                       }
                   }
            }
        return json.dumps(bgp)

    def source_name(self):
        """
        gets the name of the source router
        """
        return self.source

    def target_name(self):
        """
        gets the name of the target
        """
        return self.target
        
class NetworkScript:
    """
    A logical representation of the rDNS and all updates
    """
    
    def __init__(self, all_csv_updates):
        """
        PRE::: No more than 256 different nodes in the updates
        takes a full list of csv updates and returns a parsed Network Script
        """
        self.updates = [Update(line) for line in iter(all_csv_updates.splitlines())]
        links = [(u.source_name(), u.target_name()) for u in self.updates]
        links += [(t,s) for (s,t) in links]
        links = set(links)

        ## Generate rDNS
        self.routers = list(set([r for pair in links for r in pair ]))
        self.rDNS_json = {"routers" : []}
        self.rDNS_logical = {}
        print("routers:", self.routers)
        for i,r in enumerate(self.routers):
            ifaces = []
            for s, t in links:
                if r == s:
                    ip = "10.0." + str(i) + "." + str(self.routers.index(t))
                    revip = "10.0." + str(self.routers.index(t)) + "." + str(i)
                    ifaces += [ip]
                    if not s in self.rDNS_logical:
                        self.rDNS_logical[s] = {}
                    self.rDNS_logical[s][t] = {"source": ip, "target": revip}

            self.rDNS_json["routers"] += [{"name": r, "ifaces": ifaces }]

    def to_BGP_string(self):
        """
        returns the logical representation as a BGP string
        """
        return "\n".join([u.to_BGP_string(self.rDNS_logical) for u in self.updates])

    def to_rDNS_string(self):
        """
        returns the rDNS record as a json string
        """
        return json.dumps(self.rDNS_json)


def main():
    arg_parser = ArgumentParser(description='Accept a stream of BGP messages on standard in')
    arg_parser.add_argument('-bmp', dest='bmp', action='store', required=True,
                            help='File Path to which the script will write  the translated bmp messages')
    arg_parser.add_argument('-rdns', dest='rdns', action='store',
                            required=True, help='File Path to which the script will write the rDNS JSON ')

    settings = arg_parser.parse_args()
    csv_updates = sys.stdin.read() ## get standard input
    nws = NetworkScript(csv_updates) ## compute the script object
    bmp_msgs = nws.to_BGP_string()
    rDNS_json = nws.to_rDNS_string()

    # Write the bmp_msgs out to settings.bmp
    with open(settings.bmp, 'w+') as f:
        f.write(bmp_msgs)

    # Write the rDNS JSON to settings.rdns
    with open(settings.rdns, 'w+') as f:
        f.write(rDNS_json)
    
if __name__ == "__main__":
    main()
