set(PRJ sharpenAlpha)

set(SRC ${PRJ}.cpp)

set(OF_INCLUDE_DIRS
#  OpenFOAM finiteVolume OSspecific/POSIX lduSolvers
#  meshTools
)

set(OF_LIBS
#  finiteVolume
)

set(INCLUDE_DIRS
  ${insight_INCLUDE_DIR}
)

set(LIBS
 uniof
)

setup_exe_target_OF(${PRJ} "${SRC}" "${OF_INCLUDE_DIRS}" "${OF_LIBS}" "${INCLUDE_DIRS}" "${LIBS}" "")
