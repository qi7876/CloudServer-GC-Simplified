#src/app
add_executable(CloudServer daeServer.cc)
target_link_libraries(CloudServer ${FINAL_OBJ})