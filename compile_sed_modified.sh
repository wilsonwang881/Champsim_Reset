#!/bin/bash

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

	# Update the Makefile with gcc/g++ path

	# Update macros.
 	sed -i "s/#define simulate_with_cache_reset 0/#define simulate_with_cache_reset ${cache_setting[i]}/g" inc/operable.h
 	sed -i "s/#define simulate_with_prefetcher_reset 0/#define simulate_with_prefetcher_reset ${prefetcher_setting[i]}/g" inc/operable.h
 	sed -i "s/#define simulate_with_btb_reset 0/#define simulate_with_btb_reset ${btb_setting[i]}/g" inc/operable.h
 	sed -i "s/#define simulate_with_branch_predictor_reset 0/#define simulate_with_branch_predictor_reset ${branch_predictor_setting[i]}/g" inc/operable.h
 	sed -i "s/#define dump_ins_number_every_4m_cycles 1/#define dump_ins_number_every_4m_cycles ${dump_setting[i]}/g" src/main.cc
 
 	# Threaded compile
 	make -j 10;
 
 	# Prompt
 	echo "$tmpp_base_name" compiled;
 
 	# Rename compiled binary
 	mv bin/champsim bin/"$tmpp_base_name";

	# Revert changes made to the macros.
	sed -i "s/#define simulate_with_cache_reset ${cache_setting[i]}/#define simulate_with_cache_reset 0/g" inc/operable.h
 	sed -i "s/#define simulate_with_prefetcher_reset ${prefetcher_setting[i]}/#define simulate_with_prefetcher_reset 0/g" inc/operable.h
 	sed -i "s/#define simulate_with_btb_reset ${btb_setting[i]}/#define simulate_with_btb_reset 0/g" inc/operable.h
 	sed -i "s/#define simulate_with_branch_predictor_reset ${branch_predictor_setting[i]}/#define simulate_with_branch_predictor_reset 0/g" inc/operable.h
 	sed -i "s/#define dump_ins_number_every_4m_cycles ${dump_setting[i]}/#define dump_ins_number_every_4m_cycles 1/g" src/main.cc
done

#for ((i=0;i<${#bin_names[@]} - 17;i++)); do
#
#	#
#	#
#	# Update the Makefile with gcc/g++ path
#	sed -i 's/CC := gcc/CC := ~\/gcc_install\/usr\/local\/bin\/gcc/g' Makefile
#	sed -i 's/CXX := g++/CXX := ~\/gcc_install\/usr\/local\/bin\/g++/g' Makefile
#
#done
