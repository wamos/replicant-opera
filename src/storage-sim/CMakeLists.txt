
message("flowsim for ${FLOW_NUM} flows")
cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
set(CC_FLAGS "-Wpedantic -Wconversion -Wall")
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread -g -lrt")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CC_FLAGS}")

#set(flowsim_rotor_SRC flowsim_main.cc flowsim_topo_rotor.cc)
set(flowsim_rotor_TEST flowsim_test.cc flowsim_topo_rotor.cc)
#add_executable(flowsim_rotor ${flowsim_rotor_SRC})
add_executable(flowsim_testfn  ${flowsim_rotor_TEST})

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -g -lrt -fno-omit-frame-pointer -fsanitize=address")
#set(LINKER_FLAGS "-fsanitize=address")
#target_link_libraries(flow_sim_hadoop ${LINKER_FLAG})
#target_link_libraries(flow_sim-hadoop m Threads::Threads)
