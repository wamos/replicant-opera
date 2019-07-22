cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -g -lrt")
set(flow_sim_hadoop_SRC flow_sim_test.cc)
add_executable(flow_sim_hadoop ${flow_sim_hadoop_SRC})

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -g -lrt -fno-omit-frame-pointer -fsanitize=address")
#set(LINKER_FLAGS "-fsanitize=address")
#target_link_libraries(flow_sim_hadoop ${LINKER_FLAG})
#target_link_libraries(flow_sim-hadoop m Threads::Threads)
