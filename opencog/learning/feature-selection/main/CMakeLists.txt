ADD_EXECUTABLE (main main.cc)
TARGET_LINK_LIBRARIES(main
  feature_selection
)
SET_TARGET_PROPERTIES(main PROPERTIES OUTPUT_NAME "feature-selection")

ADD_EXECUTABLE (eval-features eval-features.cc)
TARGET_LINK_LIBRARIES(eval-features
  comboreduct
  util
  ${Boost_PROGRAM_OPTIONS_LIBRARY} 
)

# install feature-selection
INSTALL(TARGETS main eval-features RUNTIME DESTINATION bin)
