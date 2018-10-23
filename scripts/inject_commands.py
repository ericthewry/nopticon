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
    arg_parser.add_argument('-peerchange', dest='peerchange',
            action='store_true', 
            help='Print and reset network summary on every peer up/down event')
    settings = arg_parser.parse_args()

    # Open input and output streams
    istream = open(settings.input_path, 'r')
    ostream = open(settings.output_path, 'w')

    # Process input stream
    for bmp_json in istream.readlines():
        bmp_msg = bmp.parse_message(bmp_json.strip())
        if (settings.peerchange and (bmp_msg._type == bmp.MessageType.PEER_UP 
            or bmp_msg._type == bmp.MessageType.PEER_DOWN)):
            ostream.write(nopticon.Command.print_log().json()+'\n')
            ostream.write(nopticon.Command.reset_network_summary().json()+'\n')
        ostream.write(bmp_json)

    # Close input and output streams
    istream.close()
    ostream.close()

if __name__ == '__main__':
    main()
