#!/bin/bash

cd ../build

: '
echo "block_size,bucket_count,seconds"
for (( block_size = 64 * 1024 * 1024; block_size <= 512 * 1024 * 1024; block_size *= 2 )); do
    for (( bucket_count = 64; bucket_count <= 2048; bucket_count *= 2 )); do
        echo -n "$block_size,$bucket_count,"
        ./sim_bucketize --block_size $block_size --bucket_count $bucket_count | grep "seconds" | awk "{print \$(NF-1)}"
    done
done
#'

#: '
echo "block_size,seconds"
for (( block_size = 1024 * 1024 * 1024; block_size <= 16 * 1024 * 1024 * 1024; block_size *= 2 )); do
    echo -n "$block_size,"
    ./sim_sort --block_size $block_size | grep "seconds" | awk "{print \$(NF-1)}"
done
#'
