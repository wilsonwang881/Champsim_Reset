#!/bin/bash

experiment_batch_run() {
	workloadRelativeArray=()
	workloadAbsoluteArray=()
	cmdArray=()

	champsim_trace_dir="$1"
	champsim_bin_path="$2"
	champsim_bin_base_name="$3"
	champsim_reset_ins_data_dir="$4"

	for traceFile in "$champsim_trace_dir"*; do 
	       workloadAbsoluteArray+=("$traceFile");
	       workloadRelativeArray+=($(basename ${traceFile})); 
	done

	mkdir -p "$champsim_bin_base_name"

	cd "$champsim_bin_base_name";

	number_of_workloads=${#workloadRelativeArray[*]}

	for ((i = 0; i<$(( number_of_workloads)); i++)); do
	    mkdir -p "${workloadRelativeArray[$i]}";
	    cd "${workloadRelativeArray[$i]}";
	    #cp ~/Pause_Experiment_Champsim/create_and_attach_named_pipe.sh .
	    #./create_and_attach_named_pipe.sh
	    cp "$champsim_reset_ins_data_dir"/"${workloadRelativeArray[$i]}"/log.txt log_baseline.txt;
	    cp "$champsim_reset_ins_data_dir"/"${workloadRelativeArray[$i]}"/reset_ins_number.txt .;
	    job_cmd="cd $PWD && $champsim_bin_path --warmup_instructions 0 --simulation_instructions 2000000000 ${workloadAbsoluteArray[$i]} > log.txt"
	    cmdArray+=("$job_cmd");
	    cd ../;
	done

	cd ../;

	for cmd in "${cmdArray[@]}"; do
	    echo "$cmd" >> reset_jobs.txt;
	done
}

pause_experiment_root_dir="$1"
champsim_trace_dir="$2"
champsim_reset_ins_data_dir="$3"


for champsim_bin in "$pause_experiment_root_dir"bin/*; do
    tmpp_base_name=$(basename "$champsim_bin");
    if [ "$tmpp_base_name" != "sed_no_reset_record_reset_ins" ]; then
           echo "$tmpp_base_name" running;
	   experiment_batch_run "$champsim_trace_dir" "$champsim_bin" "$tmpp_base_name" "$champsim_reset_ins_data_dir";
    fi;
done

