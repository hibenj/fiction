//
// Created by benjamin on 4/8/24.
//

#include "fiction_experiments.hpp"

#include <fiction/types.hpp>                                         // pre-defined types suitable for the FCN domain

#include <filesystem>
#include <cstdint>
#include <vector>

#include <lorina/genlib.hpp>
#include <lorina/super.hpp>
#include <mockturtle/algorithms/mapper.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xmg_npn.hpp>
#include <mockturtle/generators/arithmetic.hpp>
#include <mockturtle/io/genlib_reader.hpp>
#include <mockturtle/io/super_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/networks/xmg.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/binding_view.hpp>

#include <mockturtle/io/verilog_reader.hpp>  // call-backs to read Verilog files into networks

using namespace mockturtle;

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
                                 "GATE  maj6        6 O=a*b*c*d+a*b*e*f+c*d*e*f;    PIN * INV    1 999 1.0 1.0 1.0 1.0\n"
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

    map_params ps;
    map_stats st;

    /*for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        const auto network = read_ntk<aig_network>(benchmark);
        binding_view<klut_network> ntk_map = map( network, lib, ps, &st );
        ntk_map.report_gates_usage();
    }*/

    auto files = GetVFiles( "/home/benjamin/Documents/Repositories/working/fiction/benchmarks/abc_gen" );

    for (const auto& filePath : files) {
        std::cout << filePath << std::endl;
        aig_network network;
        const auto read_verilog_result =
            lorina::read_verilog(filePath, mockturtle::verilog_reader(network));
        assert(read_verilog_result == lorina::return_code::success);
        binding_view<klut_network> ntk_map = map( network, lib, ps, &st );
        ntk_map.report_gates_usage();
    }

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