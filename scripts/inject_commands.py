#!/usr/bin/python3

"""
Inject nopticon commands into a BMP message stream 
"""

from argparse import ArgumentParser
import bmp 
import nopticon

def main():
    # Parse arguments
    arg_parser = ArgumentParser(description='Inject nopticon commands into a BMP message stream')
    arg_parser.add_argument('-input', dest='input_path', action='store',
            required=True, help='Path for BMP message stream')
    arg_parser.add_argument('-output', dest='output_path', action='store',
            required=True, help='Path for modified message stream')
    arg_parser.add_argument('-rdns', dest='rdns_path', action='store',
            default=None, help='Path to rdns JSON file')
    arg_parser.add_argument('-peerchange', dest='peerchange',
            action='store_true', 
            help='Print and reset network summary on every peer up/down event')
    arg_parser.add_argument('-end', dest='end',
            action='store_true',
            help='Print network summary at end of message stream')
    settings = arg_parser.parse_args()

    # Load rdns
    rdns = {}
    if settings.rdns_path is not None:
        with open(settings.rdns_path, 'r') as rdnsfile:
            rdns = nopticon.parse_rdns(rdnsfile.read())

    # Open input and output streams
    istream = open(settings.input_path, 'r')
    ostream = open(settings.output_path, 'w')

    # Modification-specific variables
    if (settings.peerchange):
        seen_peer_down = False
        up_links = []
        down_links = []

    # Process input stream
    for bmp_json in istream.readlines():
        print(bmp_json)
        bmp_msg = bmp.parse_message(bmp_json.strip())
        if (settings.peerchange and
                (bmp_msg.isPeerUp() or bmp_msg.isPeerDown())): 

            # Special flag to avoid inserting commands when network is still 
            # starting
            if (bmp_msg.isPeerDown()):
                seen_peer_down = True

            # Determine if link went up or down
            edge = bmp_msg.edge()
            if settings.rdns_path is not None:
                assert edge[0] in rdns, "%s not in rdns" % edge[0]
                assert edge[1] in rdns, "%s not in rdns" % edge[1]
                edge = (rdns[edge[0]], rdns[edge[1]])
            if (edge[1] < edge[0]):
                edge = (edge[1], edge[0])
            change = False
            if (bmp_msg.isPeerUp() and edge not in up_links):
                print('Up %s-%s' % edge)
                up_links.append(edge)
                if (edge in down_links):
                    down_links.remove(edge)
                change = True
            elif (bmp_msg.isPeerDown() and edge not in down_links):
                print('Down %s-%s' % edge)
                down_links.append(edge)
                if (edge in up_links):
                    up_links.remove(edge)
                change = True

            # Write commands if a link changed state
            if (seen_peer_down and change):
                ostream.write(nopticon.Command.print_log().json()+'\n')
                refresh = nopticon.Command.refresh_summary(bmp_msg._timestamp)
                ostream.write(refresh.json()+'\n')
        ostream.write(bmp_json)

    if (settings.end):
        ostream.write(nopticon.Command.print_log().json()+'\n')

    # Close input and output streams
    istream.close()
    ostream.close()

if __name__ == '__main__':
    main()
