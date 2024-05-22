#!/bin/bash

# Setup the compiler path
source server_env.sh
# Arrays that store the reset configurations.
bin_names=()
cache_setting=()

# Replace macros in source code and compile.
for SIZE in 1000 2000 3000 4000 5000 6000 7000 8000 9000 10000 
do
	# Configure Champsim.
 	./config.sh champsim_config.json

	# Get the name of the binary.
 	tmpp_base_name=${bin_names[i]}

	# Update macros.
 	sed -i "s/#define ON_DEMAND_ACCESS_RECORD_SIZE 1000/#define ON_DEMAND_ACCESS_RECORD_SIZE $SIZE/g" inc/operable.h
  
 	# Threaded compile
 	make -j 16;
 
 	# Prompt
 	echo "baseline_observation_window_size_$SIZE" compiled;
 
 	# Rename compiled binary
 	mv bin/champsim "bin/baseline_observation_window_size_$SIZE";

	# Revert changes made to the macros.
	sed -i "s/#define ON_DEMAND_ACCESS_RECORD_SIZE $SIZE/#define ON_DEMAND_ACCESS_RECORD_SIZE 1000/g" inc/operable.h
 done

