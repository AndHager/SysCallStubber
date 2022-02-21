#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

HELP="Usage: execfuzz.sh SYSTEMD_PATH LOG_PATH [OPTION]...
Executes all fuzziers from the standard or the specified conf file.

Mandatory argument:
  SYSTEMD_PATH      Path to systemd
  LOG_PATH          Output path

Optional argument:
  -et, --execution-time         Execution time of each fuzzer in seconds. SUFFIX may be 's' for seconds (the default), 'm' for minutes, 'h' for hours or 'd' for days.. (default=60)
  -cf, --configuration-file     Path to the configuration file. (default=$SCRIPT_DIR/execfuzz.conf)
  -rp, --run-parallel           Parallel run. (default: True)
  -h, --help                    Display this help and exit.

Report any bugs to Andreas Hager <andreas.hager@aisec.fraunhofer.de>
"

if (( $# < 2 )); then
  printf "$HELP"
  exit 1
fi

SYSTEMD_PATH=$1
shift # past Mandatory argument
if [ ! -d "$SYSTEMD_PATH" ]; then
  printf "__ERROR__: $SYSTEMD_PATH is not a directory"
  exit 1
fi
LOG_PATH=$1
shift # past Mandatory argument
if [ ! -d "$LOG_PATH" ]; then
  mkdir $LOG_PATH
fi

EXEC_TIME="60"
CONFIG_FILE="$SCRIPT_DIR/execfuzz.conf"
PARALLEL="True"
THREATS="20"
HELP="NO"


POSITIONAL=()
while [[ $# -gt 0 ]]; do
  key="$1"

  case $key in
    -et|--execution-time )
      EXEC_TIME="$2"
      shift # past argument
      shift # past value
      ;;
    -cf|--configuration-file)
      CONFIG_FILE="$2"
      shift # past argument
      shift # past value
      ;;
    -rp|--run-parallel)
      PARALLEL="$2"
      shift # past argument
      shift # past value
      ;;
    -h|--help)
      HELP="YES"
      shift # past argument
      ;;
    *)    # unknown option
      POSITIONAL+=("$1") # save it in an array for later
      shift # past argument
      ;;
  esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

if [ HELP == "YES" ]; then
  printf "$HELP"
  exit 0
fi

while IFS= read -r line; do
    file=$SYSTEMD_PATH/out/$line
    if [[ -x "$file" ]]
    then # file is Executable q
      echo "__INFO__: processing $file"
      printf "\n__INFO__: processing $file:\n" >> /info/ReturnValues.json
      FP=""
      if [[ "$PARALLEL" == "True" ]]
      then 
        FP="-fork=$THREATS -ignore_ooms=True -ignore_timeouts=True -ignore_crashes=True"
      fi
      $file -seed=424242442 -rss_limit_mb=0 -len_control=0 -max_len=4096 $FP -max_total_time=$EXEC_TIME -print_final_stats=1 &> $LOG_PATH/${line}_output.txt
      _ret_code=$?
      if [ $_ret_code == 0 ]; then 
        echo "    SUCESSFULL fuzz test"
      else 
        echo "    ERROR: fuzzer exit with code $_ret_code"
      fi
    fi
done < $CONFIG_FILE

exit 0
