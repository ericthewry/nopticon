#!/usr/bin/python3

"""
Get various statistics for a BMP message stream
"""

from argparse import ArgumentParser
import bmp 
import math
import nopticon

def main():
    # Parse arguments
    arg_parser = ArgumentParser(description='Get various statistics for a BMP message stream')
    arg_parser.add_argument('-input', dest='input_path', action='store',
            required=True, help='Path for BMP message stream')
    arg_parser.add_argument('-verbose', dest='verbose', action='store_true',
            default=False, help='Verbose output')
    arg_parser.add_argument('-rdns', dest='rdns_path', action='store',
            default=None, help='Path to rdns JSON file')
    arg_parser.add_argument('-duration', dest='duration', action='store_true',
            default=False, help='Get elapsed time (in seconds) between first '
            + 'and last message')
    arg_parser.add_argument('-peerevents', dest='peerevents', 
            action='store_true', default=False, help='Get the number of peer '
            + 'up/down events')
    settings = arg_parser.parse_args()

    # Load rdns
    rdns = {}
    if settings.rdns_path is not None:
        with open(settings.rdns_path, 'r') as rdnsfile:
            rdns = nopticon.parse_rdns(rdnsfile.read())

    # Declare statistics variables
    first_timestamp = 0
    last_timestamp = 0
    peer_up = 0
    peer_down = 0

    # Process input stream
    with open(settings.input_path, 'r') as istream:
        for bmp_json in istream:
            bmp_msg = bmp.parse_message(bmp_json.strip())

            # Get first and last timestamps, if required
            if settings.duration:
                if bmp_msg._timestamp != 0:
                    if first_timestamp == 0:
                        first_timestamp = bmp_msg._timestamp
                    last_timestamp = bmp_msg._timestamp
            
            # Count peer events, if requested
            if settings.peerevents:
                if (bmp_msg._type == bmp.MessageType.PEER_UP):
                    peer_up += 1
                    if settings.verbose:
                        source = (bmp_msg._src_id if bmp_msg._src_id not in rdns else rdns[bmp_msg._src_id])
                        peer = (bmp_msg._peer if bmp_msg._peer not in rdns else rdns[bmp_msg._peer])
                        print('Up: %s--%s' % (source, peer))
                if (bmp_msg._type == bmp.MessageType.PEER_DOWN):
                    peer_down += 1
                    if settings.verbose:
                        source = (bmp_msg._src_id if bmp_msg._src_id not in rdns else rdns[bmp_msg._src_id])
                        peer = (bmp_msg._peer if bmp_msg._peer not in rdns else rdns[bmp_msg._peer])
                        print('Down: %s--%s' % (source, peer))

    if settings.duration:
        duration = last_timestamp - first_timestamp
        print('Duration: %d seconds' % (math.ceil(duration)))
    if settings.peerevents:
        print('Peer Up events: %d' % (peer_up))
        print('Peer Down events: %d' % (peer_down))

if __name__ == '__main__':
    main()
