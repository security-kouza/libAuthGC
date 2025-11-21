#ifndef ATLab_BENCHMARK_HPP
#define ATLab_BENCHMARK_HPP

#ifdef ENABLE_BENCHMARK
    #define BENCHMARK_INIT std::chrono::time_point<std::chrono::system_clock> start, end;
    #define BENCHMARK(message, code) \
        start = std::chrono::high_resolution_clock::now();\
        code\
        end = std::chrono::high_resolution_clock::now();\
        std::cout << #message << ": "\
            << std::chrono::duration<double, std::milli>{end - start}.count() << "ms\n";
    #define BENCHMARK_START start = std::chrono::high_resolution_clock::now();
    #define BENCHMARK_END(message)\
        end = std::chrono::high_resolution_clock::now();\
        std::cout << #message << ": "\
            << std::chrono::duration<double, std::milli>{end - start}.count() << "ms\n";

#else
    #define BENCHMARK_INIT ;
    #define BENCHMARK(message, code) code
    #define BENCHMARK_START ;
    #define BENCHMARK_END(message) ;
#endif


#endif // ATLab_BENCHMARK_HPP
