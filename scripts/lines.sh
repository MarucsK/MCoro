#!/bin/bash
echo "co_async: $(fd '[ch]pp' co_async -x wc -l {} | awk '{print $1}' | paste -d+ -s | bc)"
echo "steps: $(fd '[ch]pp' steps -x wc -l {} | awk '{print $1}' | paste -d+ -s | bc)"
