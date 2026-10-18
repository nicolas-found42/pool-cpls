add_executable(pool_bench world_bench.cpp)
target_link_libraries(pool_bench PRIVATE pool::game)
target_compile_features(pool_bench PRIVATE cxx_std_23)
