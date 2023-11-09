#!/bin/bash

# The script generate commands that run the baseline
# and store the commands to a file.

workloadRelativeArray=()
workloadAbsoluteArray=()
cmdArray=()

champsim_trace_dir="$1"
champsim_bin_path="$2"

for traceFile in "$champsim_trace_dir"*; 
do 
       workloadAbsoluteArray+=("$traceFile");
       workloadRelativeArray+=($(basename ${traceFile})); 
done

mkdir -p "baseline"

cd "baseline";

number_of_workloads=${#workloadRelativeArray[*]}

for ((i = 0; i<$(( number_of_workloads)); i++)); 
do
       mkdir -p "${workloadRelativeArray[$i]}";
       cd "${workloadRelativeArray[$i]}";
       job_cmd="cd $PWD && $champsim_bin_path/bin/sed_no_reset_record_reset_ins --warmup-instructions 0 --simulation-instructions 2000000000 ${workloadAbsoluteArray[$i]} > log.txt"
       cmdArray+=("$job_cmd");
       cd ../
done

cd ../;

for cmd in "${cmdArray[@]}"; 
do
       echo "$cmd" >> baseline_jobs.txt;
done

