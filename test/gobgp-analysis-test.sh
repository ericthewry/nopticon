#!/usr/bin/env bash

DATA=test/data
BUILD=build

cat ${DATA}/ft4_gobgp.bmp | ${BUILD}/gobgp-analysis --verbosity 7 ${DATA}/ft4_rdns.json | grep  -n "errors" | cut -f1 -d':' | diff ${DATA}/ft4.errors -
