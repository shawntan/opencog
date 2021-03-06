ADD_LIBRARY (oac 
    Pet
    Plaza
    OAC
    PetMessageSender
    PVPActionPlanSender
    ProcedureInterpreterAgent
    ImportanceDecayAgent
    ActionSelectionAgent
    EntityExperienceAgent
    PsiModulatorUpdaterAgent
    PsiDemandUpdaterAgent
    PsiActionSelectionAgent
    PsiRelationUpdaterAgent
    PsiFeelingUpdaterAgent
    PsiRuleUtil
    StimulusUpdaterAgent
    SleepAgent
    MockOpcHCTest
    HCTestAgent
    BaseAgentModeHandler
    DefaultAgentModeHandler
    LearningAgentModeHandler
    LanguageComprehension
    FramesToRelexRuleEngine
    NLGenClient
    OutputRelex
)

INCLUDE_DIRECTORIES (
    ${XERCESC_INCLUDE_DIR}
    ${PYTHON_INCLUDE_PATH}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

SET(OAC_LINK_LIBS
    ${XERCESC_LIBRARIES}
    json_spirit
    PredicateUpdaters
    PAI
    behavior
    MessagingSystem
    Procedure
    savable
    attention
)

IF (HAVE_CYTHON)
    SET(OAC_LINK_LIBS
        ${OAC_LINK_LIBS}
        PythonEval
		PythonModule
#        logic_wrapper
		${PYTHON_LIBRARIES})
ENDIF (HAVE_CYTHON)    

IF (HAVE_ZMQ)
    SET(OAC_LINK_LIBS ${OAC_LINK_LIBS} ${ZMQ_LIBRARIES})
ENDIF (HAVE_ZMQ)

TARGET_LINK_LIBRARIES (oac ${OAC_LINK_LIBS})

ADD_EXECUTABLE (OACTester OACTester.cc)
TARGET_LINK_LIBRARIES (OACTester
    oac
    comboreduct
    AvatarComboVocabulary
    AtomSpaceExtensions
    ImaginaryLife
    ${Boost_THREAD_LIBRARY}
)

ADD_EXECUTABLE (oac-exec OACExecutable.cc)
TARGET_LINK_LIBRARIES (oac-exec
    oac
    comboreduct
    AvatarComboVocabulary
    ImaginaryLife
    ${Boost_THREAD_LIBRARY} 
    ${ZMQ_LIBRARIES}
)
SET_TARGET_PROPERTIES(oac-exec PROPERTIES OUTPUT_NAME "oac")

ADD_EXECUTABLE (opcHcTester MockOpcHCTestExec.cc)
TARGET_LINK_LIBRARIES (opcHcTester
    oac
    comboreduct
    AvatarComboVocabulary
    ImaginaryLife
)
