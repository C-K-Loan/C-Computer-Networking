#!/bin/bash

N_INSTANCES=10
N_TESTS=10
SERVER=./server
CLIENT=./client
PORTS=()
STARTPORT=1024
PROCS=()
MAX=65536 # 2^16

# start N_INSTANCES instances of the server
# each server registers with a random previous server
for ((i=0; i<$N_INSTANCES; i++))
do
    NEW_PORT=`expr $STARTPORT + $i`
    if [ $i -eq 0 ]
    then
        (./server localhost $NEW_PORT > /dev/null) &
    else
        # choose random registration port
        REGISTRATION_PORT=${PORTS[(($RANDOM % i))]}
        ID=`expr $i \* $MAX / $N_INSTANCES`
        (./server localhost $NEW_PORT localhost $REGISTRATION_PORT $i > /dev/null) &
    fi
    PORTS[$i]=$NEW_PORT
    # save last process number
    PROCS[$i]=$!
done

echo waiting for servers to stabilize chord
sleep 2
echo starting test

for ((i=0; i<N_TESTS; i++))
do
    RANDOM_PORT=${PORTS[(($RANDOM % N_INSTANCES))]]}
    echo set on port $RANDOM_PORT
    ./client localhost $RANDOM_PORT "set" "key$i$" "val$i"
    RANDOM_PORT=${PORTS[(($RANDOM % N_INSTANCES))]]}
    echo get on port $RANDOM_PORT
    ./client localhost $RANDOM_PORT "get" "key$i$" "val$i"
done

# test set/get of keys using a random server

# kill all servers
killall $SERVER
# spin-up cluster of 10 dht instances
