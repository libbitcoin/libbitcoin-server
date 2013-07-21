#!/bin/bash
cd src
make
cd ../client
make
cd ../queue
make
cd ../worker
make

