# MOSES' version, must be updated manually
SET (MOSES_VERSION_MAJOR 3)
SET (MOSES_VERSION_MINOR 2)
SET (MOSES_VERSION_PATCH 8)

ADD_DEFINITIONS(-DMOSES_VERSION_MAJOR="${MOSES_VERSION_MAJOR}")
ADD_DEFINITIONS(-DMOSES_VERSION_MINOR="${MOSES_VERSION_MINOR}")
ADD_DEFINITIONS(-DMOSES_VERSION_PATCH="${MOSES_VERSION_PATCH}")

# MOSES bzr revision number, automatically updated
EXECUTE_PROCESS(COMMAND bzr revno "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE MOSES_BZR_REVNO)
IF (MOSES_BZR_REVNO)
  ADD_DEFINITIONS(-DMOSES_BZR_REVNO="${MOSES_BZR_REVNO}")
ENDIF (MOSES_BZR_REVNO)

ADD_SUBDIRECTORY (moses)
ADD_SUBDIRECTORY (representation)
ADD_SUBDIRECTORY (optimization)
ADD_SUBDIRECTORY (eda)
ADD_SUBDIRECTORY (main)
ADD_SUBDIRECTORY (example-ant)
ADD_SUBDIRECTORY (example-progs)
ADD_SUBDIRECTORY (man)

ADD_LIBRARY (moses SHARED STATIC
	eda/initialization
	eda/local_structure
	moses/complexity
	moses/distributed_moses
	moses/local_moses
	moses/metapopulation
	moses/moses_main
	moses/mpi_moses
	moses/neighborhood_sampling
	moses/partial
	moses/pole_balancing
	moses/scoring
	moses/types
	moses/using
	optimization/hill-climbing
	optimization/optimization
	optimization/star-anneal
	optimization/univariate
	representation/build_knobs
	representation/field_set
	representation/knob_mapper
	representation/knobs
	representation/representation
)

TARGET_LINK_LIBRARIES (moses
	comboreduct
	feature_selection
	util 
	${Boost_PROGRAM_OPTIONS_LIBRARY} 

	# These two work for both MPICH2 and OpenMPI, for me.
	${MPI_LIBRARY}
	${MPI_EXTRA_LIBRARY}
)


# target_link_libraries(hello ${MPI_LIBRARIES}) 
# if(MPI_COMPILE_FLAGS) 
#   set_target_properties(hello PROPERTIES 
#     COMPILE_FLAGS "${MPI_COMPILE_FLAGS}") 
# endif() 
# 
# if(MPI_LINK_FLAGS) 
#   set_target_properties(hello PROPERTIES 
#     LINK_FLAGS "${MPI_LINK_FLAGS}") 
# endif() 

# Install library
IF (WIN32)
	INSTALL(TARGETS moses DESTINATION "lib${LIB_DIR_SUFFIX}/opencog")
ELSE (WIN32)
	INSTALL(TARGETS moses 
           LIBRARY DESTINATION "lib${LIB_DIR_SUFFIX}"    # lib*.so files
           ARCHIVE DESTINATION "lib${LIB_DIR_SUFFIX}")   # lib*.a files
ENDIF (WIN32)
