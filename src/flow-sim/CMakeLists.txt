find_package(Threads REQUIRED)

set(flow_sim_hadoop_SRC flow_sim-hadoop.cc)
add_executable(flow_sim-hadoop ${flow_sim_hadoop_SRC})
target_link_libraries(flow_sim-hadoop m Threads::Threads)
