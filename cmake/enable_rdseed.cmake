OPTION(USE_RANDOM_DEVICE "Enable RDSEED instruction" OFF)

include(CheckCSourceRuns)

IF(NOT ${USE_RANDOM_DEVICE})
	# check if rdseed available
	unset(RDSEED_COMPILE_RESULT CACHE)
	unset(RDSEED_RUN_RESULT CACHE)
	set(CMAKE_REQUIRED_FLAGS "-mrdseed") # in case CMAKE_C_FLAGS doesn't have it, ex. if add_compile_options is used instead
	check_c_source_runs("#include <x86intrin.h>\nint main(){\nunsigned long long r;\n_rdseed64_step(&r);\nreturn 0;\n}\n" RDSEED_RUN_RESULT)

	string(COMPARE EQUAL "${RDSEED_RUN_RESULT}" "1" RDSEED_RUN_SUCCESS)

	IF(${RDSEED_RUN_SUCCESS})
		ADD_DEFINITIONS(-DENABLE_RDSEED)
		message(STATUS "${Green}Source of Randomness: rdseed${ColourReset}")
	ELSE (${RDSEED_RUN_SUCCESS})
		message(WARNING "RDSEED test failed, falling back to random_device.")
		set(USE_RANDOM_DEVICE ON)
	ENDIF(${RDSEED_RUN_SUCCESS})
ENDIF(NOT ${USE_RANDOM_DEVICE})

IF (${USE_RANDOM_DEVICE})
	message(STATUS "${Red}Source of Randomness: random_device${ColourReset}")
ENDIF (${USE_RANDOM_DEVICE})
