//
// Created by benjamin on 4/8/24.
//

#include "fiction_experiments.hpp"

#include <fiction/types.hpp>  // pre-defined types suitable for the FCN domain

#include <lorina/blif.hpp>
#include <lorina/genlib.hpp>
#include <lorina/super.hpp>
#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xmg_npn.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/io/blif_reader.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/io/super_reader.hpp>
#include <mockturtle/io/verilog_reader.hpp>  // call-backs to read Verilog files into networks
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/binding_view.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

using namespace mockturtle;

#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

std::string trim(const std::string& str){
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

std::string extractModuleName(const std::string& fileName){
    std::ifstream file(fileName);
    std::string line;
    std::string moduleName;

    if(file.is_open()){
        int count = 4; // Number of lines to read
        while(count-- && getline(file, line)){
            std::istringstream iss(line);
            std::string token;
            if(getline(iss, token, ' ') && token == "module"){
                if(getline(iss, moduleName, '(')){
                    moduleName = trim(moduleName); // Trim leading and trailing spaces
                    if(!moduleName.empty() && moduleName[0] == '\\') {
                        moduleName.erase(0, 1); // If module name starts with \, remove it
                    }
                }
                break;
            }
        }
        file.close();
    } else {
        std::cout << "Unable to open file.\n";
    }

    return moduleName;
}

std::vector<std::string> GetVFiles(const std::string& path) {
    std::vector<std::string> files;
    for(const auto & entry : std::filesystem::directory_iterator(path)) {
        if(entry.is_regular_file() && entry.path().extension() == ".v") {
            std::string fullPath = entry.path().string();
            files.push_back(fullPath);
        }
    }
    return files;
}

std::vector<std::string> GetBlifFiles(const std::string& path) {
    std::vector<std::string> files;
    for(const auto & entry : std::filesystem::directory_iterator(path)) {
        if(entry.is_regular_file() && entry.path().extension() == ".blif") {
            std::string fullPath = entry.path().string();
            files.push_back(fullPath);
        }
    }
    return files;
}

template <typename Ntk>
Ntk read_ntk(const std::string& name)
{
    fmt::print("[i] processing {}\n", name);

    Ntk network{};

    const auto read_verilog_result =
        lorina::read_verilog(fiction_experiments::benchmark_path(name), mockturtle::verilog_reader(network));
    assert(read_verilog_result == lorina::return_code::success);

    return network;
}

std::string const test_library = "GATE  zero        0  O=CONST0;\n"
                                 "GATE  one         0  O=CONST1;\n"
                                 "GATE  buf         1 O=a;                          PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  inv1        1 O=!a;                         PIN * INV    1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  and2        1 O=a*b;                        PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  nand2       1 O=!(a*b);                     PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  or2         1 O=a+b;                        PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  nor2        1 O=!(a+b);                     PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  xor2        1 O=a^b;                        PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  xnor2       1 O=!(a^b);                     PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  and3        1 O=a*b*c;                      PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  xor_and     1 O=a*(b^c);                    PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  or_and      1 O=a*(b+c);                    PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  maj3        1 O=a*b+a*c+b*c;                PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  maj6_0      1 O=(a*b*c*d)+(a*b*e*f)+(c*d*e*f);    PIN * INV    1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  maj6_1      1 O=(a*b*c)+(a*d*e)+(b*c*d*e);        PIN * INV    1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  maj6_2      1 O=(a*b)+(a*c*d)+(b*c*d);            PIN * INV    1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  maj6_3      1 O=(a*b)+(c*d);                    PIN * INV    1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  maj6_4      1 O=a+(b*c);                      PIN * INV    1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  mux  1 O=(a*!c)+(b*c);  PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  and_xor  1 O=a^(b*c);  PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n";

std::string const test_library_ = "GATE  zero        0  O=CONST0;\n"
                                 "GATE  one         0  O=CONST1;\n"
                                 "GATE  buf         1 O=a;                          PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  inv1        1 O=!a;                         PIN * INV    1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  and2        1 O=a*b;                        PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  nand2       1 O=!(a*b);                     PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  or2         1 O=a+b;                        PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  nor2        1 O=!(a+b);                     PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  xor2        1 O=a^b;                        PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  xnor2       1 O=!(a^b);                     PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  and3        1 O=a*b*c;                      PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  xor_and     1 O=a*(b^c);                    PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  or_and      1 O=a*(b+c);                    PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  maj3        1 O=a*b+a*c+b*c;                PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  mux  1 O=(a*!c)+(b*c);  PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n"
                                 "GATE  and_xor  1 O=a^(b*c);  PIN * NONINV 1 999 1.0 1.0 1.0 1.0\n";

int main()
{
    constexpr const uint64_t bench_select = fiction_experiments::all;

    std::vector<gate> gates;

    std::istringstream in( test_library );
    auto result = lorina::read_genlib( in, genlib_reader( gates ) );
    assert( result == lorina::return_code::success );

    tech_library<6> lib( gates );

    std::vector<gate> gates_;

    std::istringstream in_( test_library_ );
    auto result_ = lorina::read_genlib( in_, genlib_reader( gates_ ) );
    assert( result_ == lorina::return_code::success );

    tech_library<3> lib_( gates_ );

    map_params ps;
    map_stats st;

    // fiction benchmarks
    int count = 0;
    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        const auto network = read_ntk<aig_network>(benchmark);
        binding_view<klut_network> ntk_map = map( network, lib, ps, &st );
        ntk_map.report_gates_usage();
        std::cout << "Area: " << st.area << std::endl;
        std::cout << "Delay: " << st.delay << std::endl;
        ++count;
    }
    std::cout << count << std::endl;
    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        const auto network = read_ntk<aig_network>(benchmark);
        binding_view<klut_network> ntk_map = map( network, lib_, ps, &st );
        ntk_map.report_gates_usage();
        std::cout << "Area: " << st.area << std::endl;
        std::cout << "Delay: " << st.delay << std::endl;
    }
    // IWLS93 benchmarks
    /*auto files_iwls = GetVFiles( "/home/benjamin/Documents/Repositories/working/fiction/benchmarks/IWLS93" );

    for (const auto& filePath : files_iwls) {
        std::cout << filePath << std::endl;
        klut_network network;
        std::string top_module = extractModuleName(filePath);
        const auto read_verilog_result =
            lorina::read_verilog(filePath, mockturtle::verilog_reader(network, top_module));
        assert(read_verilog_result == lorina::return_code::success);
        binding_view<klut_network> ntk_map = map( network, lib, ps, &st );
        ntk_map.report_gates_usage();
    }*/

    // abd generated benchmarks
    /*auto files_abc = GetVFiles( "/home/benjamin/Documents/Repositories/working/fiction/benchmarks/abc_gen" );

    for (const auto& filePath : files_abc) {
        std::cout << filePath << std::endl;
        aig_network network;
        std::string top_module = extractModuleName(filePath);
        std::cout << "!" << top_module << "!" << std::endl;
        const auto read_verilog_result =
            lorina::read_verilog(filePath, mockturtle::verilog_reader(network, top_module));
        assert(read_verilog_result == lorina::return_code::success);
        std:: cout << "num_gates" << network.num_gates() << std::endl;
        binding_view<klut_network> ntk_map = map( network, lib, ps, &st );
        ntk_map.report_gates_usage();
    }*/

    // lgsynth91 benchmarks
    /*auto files = GetBlifFiles( "/home/benjamin/Documents/Repositories/original/hdl-benchmarks/hdl/mcnc/Combinational/blif" );

    for (const auto& filePath : files) {
        std::cout << filePath << std::endl;
        klut_network network;
        const auto read_verilog_result =
            lorina::read_blif(filePath, mockturtle::blif_reader(network));
        assert(read_verilog_result == lorina::return_code::success);
        binding_view<klut_network> ntk_map = map( network, lib, ps, &st );
        ntk_map.report_gates_usage();
    }*/

    aig_network aig;
    const auto a = aig.create_pi();
    const auto b = aig.create_pi();
    const auto c = aig.create_pi();
    const auto d = aig.create_pi();
    const auto e = aig.create_pi();
    const auto f = aig.create_pi();

    const auto a1 = aig.create_and( a, b );
    const auto a2 = aig.create_and( c, d );
    const auto a3 = aig.create_and( e , f );

    const auto g = aig.create_maj( a1, a2, a3 );

    aig.create_po( g );

    binding_view<klut_network> luts = map( aig, lib, ps, &st );

    luts.report_gates_usage();
}