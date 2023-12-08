#!/bin/bash
# Run script for Homework 1 EECS 583 Fall 2023
# Place this script inside the benchmark directory. e.g. benchmark1/run.sh
# Usage: sh run.sh <benchmark_name>
# where <benchmark_name> = simple OR anagram OR compress

# ACTION REQUIRED: Ensure that the path to the library and pass name are correct.
PATH2LIB="../../build/SuperblockFormationPass/SuperblockFormationPass.so"        # Specify your build directory in the project

# ACTION NEEDED: Choose the correct pass when running.
PASS=superblock_pass   

BENCH=${1}.c

# Delete outputs from previous runs. Update this if you want to retain some files across runs.
rm -f default.profraw *_prof *_fplicm *.bc *.profdata *_output *.ll *.in *.in.Z

# Convert source code to bitcode (IR).
clang -emit-llvm -c ${BENCH} -Xclang -disable-O0-optnone -o ${1}.bc

# Canonicalize natural loops (Ref: llvm.org/doxygen/LoopSimplify_8h_source.html)
opt -passes='loop-simplify' ${1}.bc -o ${1}.ls.bc

# Instrument profiler passes. Generates profile data.
opt -passes='pgo-instr-gen,instrprof' ${1}.bc -o ${1}.prof.bc

# Generate binary executable with profiler embedded
clang -fprofile-instr-generate ${1}.prof.bc -o ${1}_prof

# When we run the profiler embedded executable, it generates a default.profraw file that contains the profile data.
# if [ "${1}" = "anagram" ]; then 
# ./${1}_prof words < input.in > /dev/null 2>&1
# elif [ "${1}" = "compress" ];then
# ./${1}_prof compress.in > /dev/null 
# else
# ./${1}_prof > /dev/null
# fi

# When we run the profiler embedded executable, it generates a default.profraw file that contains the profile data.
./${1}_prof > correct_output

# Converting it to LLVM form. This step can also be used to combine multiple profraw files,
# in case you want to include different profile runs together.
llvm-profdata merge -o ${1}.profdata default.profraw

# The "Profile Guided Optimization Instrumentation-Use" pass attaches the profile data to the bc file.
opt -passes="pgo-instr-use" -o ${1}.profdata.bc -pgo-test-profile-file=${1}.profdata < ${1}.bc

# Uncomment this and disable the cleanup if you want to "see" the instumented IR.
# llvm-dis ${1}.profdata.bc -o ${1}.prof.ll

# Runs your pass on the instrumented code.
#opt --disable-output -load-pass-plugin="${PATH2LIB}" -passes="${PASS}" ${1}.profdata.bc


# We now use the profile augmented bc file as input to your pass.
opt -load-pass-plugin="${PATH2LIB}" -passes="${PASS}" ${1}.profdata.bc -o ${1}.fplicm.bc > /dev/null

# Generate binary excutable before FPLICM: Unoptimzed code
clang ${1}.ls.bc -o ${1}_no_fplicm 
# Generate binary executable after FPLICM: Optimized code
clang ${1}.fplicm.bc -o ${1}_fplicm

# Produce output from binary to check correctness
./${1}_fplicm > fplicm_output



# Cleanup: Remove this if you want to retain the created files.
rm -f *.in *.in.Z default.profraw *_prof *_fplicm *.bc *.profdata *_output *.ll words
