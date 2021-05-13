#!/bin/bash
# Runs a ROSE Tool against a source file.
# Parm $1 is a bash script to be sourced, relative to the current dir,
# that sets source_path and RUN_IT_ARGS

# Find ourselves:
rel_enclosing_dir=`dirname $0`
export TEST_SCRIPT_DIR=`(cd ${rel_enclosing_dir}; pwd)`
export ROSE_ROSE_SCRIPT_DIR=`(cd ${rel_enclosing_dir}/../../ROSE; pwd)`

# Get parms:
parm_source_script=$1

# Declares and sets:
#   log_...
#   set_strict
#   use_latest_gcc_rose
#   use_latest_intel_rose
source ${ROSE_ROSE_SCRIPT_DIR}/declare_install_functions.sh
log_start

# Sets:
#   CC
#   CXX
#   ROSE_BACKEND_CXX
#   ROSE_HOME
#   ROSE_LD_LIBRARY_PATH
#   ROSE_TOOL
use_latest_gcc_rose
#print_rose_vars

# Sets:
#   source_path
#   RUN_IT_ARGS
source ${parm_source_script}

source_dir=`dirname ${source_path}`
source_path_no_suffix=${source_path%.*}
object_path=${source_path_no_suffix}.o

# /bin/ is executable in ROSE install dir:
# /tutorial/ is libtool script in ROSE build dir:
export ROSE_TOOL_PATH=${ROSE_HOME}/bin/identityTranslator

#export MPICH_CXX=${ROSE_TOOL_PATH}
#export CXX="/usr/tce/packages/mvapich2/mvapich2-2.2-gcc-4.9.3/bin/mpic++"
#export CXX="/usr/tce/packages/mvapich2/mvapich2-2.2-gcc-4.9.3/bin/mpic++ -echo"
#export CXX="/usr/tce/packages/mvapich2/mvapich2-2.2-gcc-4.9.3/bin/mpic++ -cxx=${ROSE_TOOL_PATH}"
#export CXX=/usr/dnta/kull/developers/tools/compilers/mvapich2-2.2/gcc-4.9.3p/mpicxx
#export ROSE_CXX=${CXX}
export ROSE_CXX=${ROSE_TOOL_PATH}

_show_source () {
  log "$1:"
  cat $1
  log "End $1."
  log_separator_1
}

show_sources () {
  log_separator_1
  log "Sources:"
  log_separator_1
  for source in ${source_path} ${source_path_no_suffix}-*.[ch]*
  do
    if [ -f ${source} ]
    then
      _show_source ${source}
    fi
  done
  log "End sources"
  log_separator_1
}

count_preprocessed_lines () {
  # Takes a little while to run, so the log function below is separate 
  # in case of multiple calls.
  #  Using the ROSE tool and not the compiler because compiler can get:
  # ... cannot open source file "mpi.h": is a directory
  #     #include <mpi.h>
  #  /usr/tce/packages/intel/intel-16.0.3/bin/icc 

  log "Counting preprocessed lines..."
  preprocessed_line_count=`run_it \
  ${ROSE_CXX} \
  -E \
  | wc -l`
  status=$?
  return ${status}
}

log_preprocessed_line_count () {
  log_separator_1
  log "Preprocessed line count: $preprocessed_line_count"
  status=$?
  log_separator_1
  return ${status}
}

run_tool () {
  log "RUNNING ROSE TOOL"
#  log "MPICH_CXX=${MPICH_CXX}"
  run_it \
  log_then_run \
  ${ROSE_CXX} \
  --edg:no_warnings \
  -c \
  -o ${object_path} 
  status=$?
  return ${status}
}

run_compiler () {
  log "RUNNING COMPILER BEFORE RUNNING TOOL"
  run_it \
  log_then_run \
  /usr/tce/packages/mvapich2/mvapich2-2.2-clang-4.0.0/bin/mpic++ \
  -c \
  -o ${object_path} 
  status=$?
  return ${status}
}

run_it () {
  last_err_status=0
  $@ \
  ${RUN_IT_ARGS} \
  -I${source_dir} \
  ${source_path}
  status=$?
  if [ ${status} -ne 0 ]
  then
    last_err_status=${status}
  fi
  return ${status}
}

set_memory_limit () {
  # rzgenie nodes have 36 cores and 128G memory.  Don't take more than our share:
  # Don't use log_then_run. so we don't get unwanted "status=" lines:
  limit_cmd="ulimit -v 3000000" # 3G
  log "Running ${limit_cmd}"
  ${limit_cmd}
  status=$?
  # Note:  Normally ulimit -v is "unlimited".  If you set it to a number, then try 
  # to set it to unlimited again, or to a larger number, you get (on rzgenie 2018/05/23):
  # "bash: ulimit: virtual memory: cannot modify limit: Operation not permitted"
  return ${status}
}

set_memory_limit
show_sources
count_preprocessed_lines
log_preprocessed_line_count
##run_tool
run_compiler && run_tool
log_preprocessed_line_count

exit ${last_err_status}

