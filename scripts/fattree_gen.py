#! /usr/bin/python3

from argparse import ArgumentParser
import json


def main():
    arg_parser = ArgumentParser(description="generate a policies file for fattree -n")
    arg_parser.add_argument('n', type=int, help="Fattree size")

    settings = arg_parser.parse_args()

    policies = {}
    policies["policies"] = []
    for i in range(settings.n):
        for j in range(int(settings.n / 2)):
            for si in range(settings.n):
                for sj in range(int(settings.n / 2)):
                    if j == sj and i == si :
                        continue
                    policies["policies"].append(
                        {
                            "type"   : "reachability",
                            "source" : "leaf" + str(si) + "_" + str(sj),
                            "target" : "leaf" + str(i)  + "_" + str(j),
                            "flow"   : "3."   + str(i)  + "." + str(j) + "." + "0/24"
                            
                        }
                    )
                    # policies["policies"].append(
                    #     {
                    #         "type"   : "reachability",
                    #         "source" : "agg" + str(si) + "_" + str(sj),
                    #         "target" : "leaf" + str(i)  + "_" + str(j),
                    #         "flow"   : "3."   + str(i)  + "." + str(j) + "." + "0/24"
                            
                    #     }
                    # )

    # for i in range(2 ** int(settings.n / 2)):
    #     core = "core" + str(i)
    #     for si in range(settings.n):
    #         if i == si :
    #             continue
    #         for sj in range(int(settings.n / 2)):
    #             if j == sj:
    #                 continue
    #             policies["policies"].append(
    #                 {
    #                     "type"   : "reachability",
    #                     "source" : core,
    #                     "target" : "leaf" + str(i)  + "_" + str(j),
    #                     "flow"   : "3."   + str(i)  + "." + str(j) + "." + "0/24"
                        
    #                     }
    #             )

        

    print(json.dumps(policies, sort_keys=True, indent=4))
                    

if __name__ == "__main__":
    main()
