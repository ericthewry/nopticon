"""
Python classes for Nopticon 
"""

from enum import Enum
import ipaddress
import json

class NetworkSummary:
    def __init__(self, summary_json):
        self._summary = json.loads(summary_json)

        # Extract edges
        self._edges = {}
        for flow in self._summary['network-summary']:
            flow_prefix = ipaddress.ip_network(flow['flow'])
            flow_edges = {}
            for edge_details in flow['edges']:
                edge = (edge_details['source'], edge_details['target'])
                flow_edges[edge] = edge_details
            self._edges[flow_prefix] = flow_edges

    def get_edges(self, flow):
        if flow not in self._edges:
            return {}
        return self._edges[flow]

class CommandType(Enum):
    RESET_NETWORK_SUMMARY = 0
    PRINT_LOG = 1

class Command():
    def __init__(self, cmd_type):
        self._type = cmd_type

    def json(self):
        return json.dumps({'Command' : self._type.value})

    @classmethod
    def reset_network_summary(cls):
        return cls(CommandType.RESET_NETWORK_SUMMARY)

    @classmethod
    def print_log(cls):
        return cls(CommandType.PRINT_LOG)

def parse_policies(policies_json):
    policies_dict = json.loads(policies_json)
    policies = []
    for policy_dict in policies_dict['policies']:
        if policy_dict['type'] == 'reachability':
            policies.append(ReachabilityPolicy(policy_dict))
    return policies

class Policy:
    def __init__(self, policy_dict):
        self._type = policy_dict['type']
        self._flow = ipaddress.ip_network(policy_dict['flow'])

class ReachabilityPolicy(Policy):
    def __init__(self, policy_dict):
        assert policy_dict['type'] == 'reachability'
        super().__init__(policy_dict)
        self._source = policy_dict['source'][:10]
        self._target = policy_dict['target'][:10]

    def __str__(self):
        return '%s %s->%s' % (self._flow, self._source, self._target)

