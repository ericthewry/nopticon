"""
Python classes for Nopticon 
"""

import ipaddress
import json

class NetworkSummary:
    def __init__(self, summary_json):
        self._summary = json.loads(summary_json)

        # Find flows
        self._edges = {}
        for key in self._summary.keys():
            if '/' in key:
                network = ipaddress.ip_network(key)
                flow_edges = {}
                for edge_details in self._summary[key]:
                    edge = (edge_details['source'], edge_details['target'])
                    flow_edges[edge] = edge_details
                self._edges[network] = flow_edges

    def get_edges(self):
        return self._edges
