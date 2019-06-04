#! /usr/bin/python3

import sys
from argparse import ArgumentParser



def main():
    args = ArgumentParser(description='Accept a stream of newline separated messages. Print only the final message')
    
    last_line = ""
    for line in sys.stdin:
        last_line = line

    print(last_line)
        

if __name__ == "__main__":
    main()
