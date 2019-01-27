#! /usr/bin/python3

import sys


def main():
    graph = None
    for line in sys.stdin.readlines():
        if len(line) > 1:
            graph = eval(line)

    edges = []
    for r in graph["reach-summary"]:
        if r["flow"] == "3.0.0.0/24":
           for edge in r["edges"]:
               edges.append((edge["source"], edge["target"], round(edge["rank-0"], 2)))

    edges.sort(key = (lambda t: -1*t[-1]))

    for e in edges:
        print(*e, sep=" ")
    
    
                            
if __name__ == "__main__":
    main()
