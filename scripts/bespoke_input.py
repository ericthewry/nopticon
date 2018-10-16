#! /usr/local/bin/python

import json
import sys

SRC = 1
TGT = 2
IP_PREF = 0
TIME = 3

class Update:
    """
    Internal representation of an individual BGP Update or Delta-net Update
    """

    def __init__(self, update_str):
        """
        Parses a Logical Rule Update from a delta-net string

        Expected Format is
        +<subnet_prefix>,<source_ip>,<next_hop_ip>,<timestamp>
        or
        -<subnet_prefix>,<source_ip>,*,<timestamp>
        """
        self.is_widthdraw = update_str[0] == "-"
        fields = update_str[1:].strip().split(",")
        assert(len(fields) == 4)
        self.source = fields[SRC]
        self.target = fields[TGT]
        assert(self.is_widthdraw == (self.target == "*"))
        self.ip_prefix = fields[IP_PREF]
        self.timestamp = fields[TIME]
        
    
    def to_BGP_string(self):
        """
        Returns a string representing the rule update in GoBGP format
        """
        if self.is_widthdraw:
            bgp = {"Header": {"Type" : 0},
                   "PeerHeader" : {"PeerBGPID" : self.source, "Timestamp" : int(self.timestamp)},
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
            bgp = {"Header": {"Type" : 0},
                   "PeerHeader" : {"PeerBGPID" : self.source, "Timestamp" : int(self.timestamp)},
                   "Body" : {
                       "BGPUpdate" : {
                           "Body" : {
                               "PathAttributes" : [{"type" : 3, "nexthop" : self.target}],
                               "NLRI" : [{"prefix" :  self.ip_prefix }],
                               "WithdrawnRoutes" : []
                           }
                       }
                   }
            }
        return json.dumps(bgp)
        
        
    def to_deltanet_string(self):
        """
        Returns a string representing the rule update in DeltaNet Format
        """
        return ",".join([self.ip_prefix, self.source, self.target, self.timestamp]) + "\n"


class NetworkScript:
    """
    A logical representation of the rDNS and all updates
    """
    
    def __init__(self, all_deltanet_updates):
        """
        takes a full list of deltanet-style updates and returns a parsed Network Script
        """
        self.updates = [Update(line) for line in iter(all_deltanet_updates.splitlines())]
        

    def to_BGP_string(self):
        """
        returns the logical representation as a BGP string
        """
        return "\n".join([u.to_BGP_string() for u in self.updates])


def main():
    dn_updates = sys.stdin.read() ## get standard input
    nws = NetworkScript(dn_updates) ## compute the script object
    print nws.to_BGP_string() ## print the result


if __name__ == "__main__":
    main()
