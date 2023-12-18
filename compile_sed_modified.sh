#!/bin/bash

# Setup the compiler path
source server_env.sh
# Arrays that store the reset configurations.
bin_names=()
cache_setting=()
prefetcher_setting=()
btb_setting=()
branch_predictor_setting=()
dump_setting=()

# Read the .csv file that contains the reset configurations.
while IFS=",", read -r bin_name cache prefetcher btb branch_predictor dump
do
	bin_names+=("$bin_name")
	cache_setting+=("$cache")
	prefetcher_setting+=("$prefetcher")
	btb_setting+=("$btb")
	branch_predictor_setting+=("$branch_predictor")
	dump_setting+=("$dump")
done <  <(tail -n +2 reset_compile_configs.csv)

# Replace macros in source code and compile.
for ((i=0;i<${#bin_names[@]};i++));
do
	# Configure Champsim.
 	./config.sh champsim_config.json

	# Get the name of the binary.
 	tmpp_base_name=${bin_names[i]}

	# Update macros.
 	sed -i "s/#define SIMULATE_WITH_CACHE_RESET 0/#define SIMULATE_WITH_CACHE_RESET ${cache_setting[i]}/g" inc/operable.h
 	sed -i "s/#define SIMULATE_WITH_PREFETCHER_RESET 0/#define SIMULATE_WITH_PREFETCHER_RESET ${prefetcher_setting[i]}/g" inc/operable.h
 	sed -i "s/#define SIMULATE_WITH_BTB_RESET 0/#define SIMULATE_WITH_BTB_RESET ${btb_setting[i]}/g" inc/operable.h
 	sed -i "s/#define SIMULATE_WITH_BRANCH_PREDICTOR_RESET 0/#define SIMULATE_WITH_BRANCH_PREDICTOR_RESET ${branch_predictor_setting[i]}/g" inc/operable.h
 	sed -i "s/#define DUMP_INS_NUMBER_EVERY_4M_CYCLES 1/#define DUMP_INS_NUMBER_EVERY_4M_CYCLES ${dump_setting[i]}/g" inc/champsim.h
 
 	# Threaded compile
 	make -j 16;
 
 	# Prompt
 	echo "$tmpp_base_name" compiled;
 
 	# Rename compiled binary
 	mv bin/champsim bin/"$tmpp_base_name";

	# Revert changes made to the macros.
	sed -i "s/#define SIMULATE_WITH_CACHE_RESET ${cache_setting[i]}/#define SIMULATE_WITH_CACHE_RESET 0/g" inc/operable.h
 	sed -i "s/#define SIMULATE_WITH_PREFETCHER_RESET ${prefetcher_setting[i]}/#define SIMULATE_WITH_PREFETCHER_RESET 0/g" inc/operable.h
 	sed -i "s/#define SIMULATE_WITH_BTB_RESET ${btb_setting[i]}/#define SIMULATE_WITH_BTB_RESET 0/g" inc/operable.h
 	sed -i "s/#define SIMULATE_WITH_BRANCH_PREDICTOR_RESET ${branch_predictor_setting[i]}/#define SIMULATE_WITH_BRANCH_PREDICTOR_RESET 0/g" inc/operable.h
 	sed -i "s/#define DUMP_INS_NUMBER_EVERY_4M_CYCLES ${dump_setting[i]}/#define DUMP_INS_NUMBER_EVERY_4M_CYCLES 1/g" inc/champsim.h
done

