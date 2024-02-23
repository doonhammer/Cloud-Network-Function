#!/bin/bash
#
#
#
# Setup error handling, 
set -o errexit
set -o nounset
umask 077
#
# Read global configuration data
source "$(dirname "$0")/global_config.sh"
#
opt_test_type="NULL"
opt_test_number="NULL"
#
# Logging to file
#
echo 'Creating test' &> "$TEST_LOG"
#
# Error Handling
#
handle_error(){
    echo "$1" >&2
    exit 1
}


create_veth_setup(){

}

create_linux_bridge_setup(){

}

cleanup_test(){

}

list_tests(){

}

run_test_1(){

}
#
# CLI Usage function
#
usage() {
cat << EOF
Usage: ${0##*/} [-h] [-c type] [-d] [-s] [-l] [-t number or a]

 
     -h 	Display this help and exit
     -c 	Create test environment
     -d 	Delete test environment
     -s 	Show test environment
     -l 	List all test available
     -t 	Run Test number (a for all)

EOF
}
#
# Parse Command Line Arguments
#
while getopts "hc:dlst:" opt; do
    case "$opt" in
        h)
            usage
            exit 0
            ;;
        c) opt_test_type=$OPTARG
           ;;
        d) delete_test
           ;;
        s) show_test
		   ;;
		l) list_tests
		   ;;
		t) opt_test_number=$OPTARG
		   ;;
        '?')
           usage
           exit 1
           ;;
    esac
done

#printf '\nCreating test type: %s\n' "$opt_test_type"
if [ "$opt_test_type" != "NULL" ]; then
	#printf '\nCreating test type: %s\n' "$opt_test_type"
	create_test
    exit 1
fi

if [ "$opt_test_number" != "NULL" ]; then
	#printf '\nRunning test number type: %s\n' "$opt_test_number"
	run_test
    exit 1
fi

usage
#

# End of Script
#