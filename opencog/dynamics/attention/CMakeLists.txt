# To make Boost ublas much more efficient.
ADD_DEFINITIONS(-DNDEBUG)

#SET_DIRECTORY_PROPERTIES(PROPERTIES CLEAN_NO_CUSTOM true)
OPENCOG_ADD_ATOM_TYPES(atom_types.script atom_types.h atom_types.definitions atom_types.inheritance)
ADD_CUSTOM_TARGET(attention_atom_types DEPENDS atom_types.h)

ADD_LIBRARY(attention SHARED
	atom_types
	AttentionModule
	ForgettingAgent
	HebbianUpdatingAgent
	ImportanceSpreadingAgent
	ImportanceDiffusionAgent
	ImportanceUpdatingAgent
	STIDecayingAgent
)

INCLUDE_DIRECTORIES(
	${PROJECT_SOURCE_DIR}/opencog/util
	${PROJECT_SOURCE_DIR}/opencog/atomspace
	${PROJECT_SOURCE_DIR}/opencog/server
	${PROJECT_SOURCE_DIR}/opencog/query
)

TARGET_LINK_LIBRARIES(attention server)

IF (CYGWIN)
	INSTALL (TARGETS attention
		DESTINATION "lib${LIB_DIR_SUFFIX}/opencog"
	)
ELSE (CYGWIN)
	INSTALL (TARGETS attention
		LIBRARY DESTINATION "lib${LIB_DIR_SUFFIX}/opencog"
	)
ENDIF (CYGWIN)


INSTALL (FILES
	atom_types.h
	AttentionModule.h
	ForgettingAgent.h
	HebbianUpdatingAgent.h
	ImportanceSpreadingAgent.h
	ImportanceDiffusionAgent.h
	ImportanceUpdatingAgent.h
	STIDecayingAgent.h
	DESTINATION "include/${PROJECT_NAME}/dynamics/attention"
)
