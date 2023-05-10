//
// Created by marcel on 24.05.21.
//

#ifndef FICTION_NETWORK_BLUEPRINTS_HPP
#define FICTION_NETWORK_BLUEPRINTS_HPP

#include <mockturtle/views/names_view.hpp>

namespace blueprints
{

template <typename Ntk>
mockturtle::names_view<Ntk> maj1_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto a = ntk.create_pi("a");
    const auto b = ntk.create_pi("b");
    const auto c = ntk.create_pi("c");

    const auto m = ntk.create_maj(a, b, c);

    ntk.create_po(m, "f");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj4_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto a = ntk.create_pi("a");
    const auto b = ntk.create_pi("b");
    const auto c = ntk.create_pi("c");
    const auto d = ntk.create_pi("d");
    const auto e = ntk.create_pi("e");

    const auto m1 = ntk.create_maj(a, b, c);
    const auto m2 = ntk.create_maj(b, c, d);
    const auto m3 = ntk.create_maj(c, d, e);
    const auto m4 = ntk.create_maj(m1, m2, m3);

    ntk.create_po(m1, "m1");
    ntk.create_po(m2, "m2");
    ntk.create_po(m3, "m3");
    ntk.create_po(m4, "m4");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> unbalanced_and_inv_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto a = ntk.create_pi("a");
    const auto b = ntk.create_pi("b");

    const auto n = ntk.create_not(b);
    const auto f = ntk.create_and(a, n);

    ntk.create_po(f, "f");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> and_or_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto a = ntk.create_pi("a");
    const auto b = ntk.create_pi("b");

    const auto f1 = ntk.create_and(a, b);
    const auto f2 = ntk.create_or(a, b);

    ntk.create_po(f1, "f1");
    ntk.create_po(f2, "f2");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> multi_output_and_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto a = ntk.create_pi("a");
    const auto b = ntk.create_pi("b");

    const auto f = ntk.create_and(a, b);

    ntk.create_po(f, "f1");
    ntk.create_po(f, "f2");
    ntk.create_po(f, "f3");
    ntk.create_po(f, "f4");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> nary_operation_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();
    const auto x3 = ntk.create_pi();
    const auto x4 = ntk.create_pi();
    const auto x5 = ntk.create_pi();

    const auto f1 = ntk.create_nary_and({x1, x2, x3});
    const auto f2 = ntk.create_nary_or({x2, x3, x4});
    const auto f3 = ntk.create_nary_xor({x3, x4, x5});

    ntk.create_po(f1);
    ntk.create_po(f2);
    ntk.create_po(f3);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> constant_gate_input_maj_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();

    const auto a1 = ntk.create_and(x1, x2);
    const auto m1 = ntk.create_maj(x1, x2, ntk.get_constant(true));
    const auto a2 = ntk.create_and(ntk.create_not(a1), m1);

    ntk.create_po(a2);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> half_adder_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("a");
    const auto x2 = ntk.create_pi("b");

    const auto sum = ntk.create_and(x1, x2);
    const auto c   = ntk.create_xor(x1, x2);

    ntk.create_po(sum, "sum");
    ntk.create_po(c, "carry");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> full_adder_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("a");
    const auto x2 = ntk.create_pi("b");
    const auto x3 = ntk.create_pi("cin");

    const auto w1 = ntk.create_and(x1, x2);
    const auto w2 = ntk.create_xor(x1, x2);
    const auto w3 = ntk.create_and(x3, w2);

    const auto c   = ntk.create_or(w1, w3);
    const auto sum = ntk.create_xor(x3, w2);

    ntk.create_po(sum, "sum");
    ntk.create_po(c, "carry");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> mux21_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();
    const auto x3 = ntk.create_pi();

    const auto n1  = ntk.create_not(x3);
    const auto a1  = ntk.create_and(x1, n1);
    const auto a2  = ntk.create_and(x2, x3);
    const auto mux = ntk.create_or(a1, a2);

    ntk.create_po(mux);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> test_inverter_substitution()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();
    const auto x3 = ntk.create_pi();

    const auto n1  = ntk.create_not(x3);
    const auto a1  = ntk.create_and(x1, n1);
    const auto a2  = ntk.create_and(x2, n1);
    const auto mux = ntk.create_or(a1, a2);

    ntk.create_po(mux);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> test_inv_flag_a()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x2 = ntk.create_pi();
    const auto x1 = ntk.create_pi();

    const auto n1  = ntk.create_not(x1);
    const auto a1  = ntk.create_and(n1, x2);

    ntk.create_po(a1);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> test_inv_flag_b()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x2 = ntk.create_pi();
    const auto x1 = ntk.create_pi();

    const auto x3 = ntk.create_pi();
    const auto x4 = ntk.create_pi();

    const auto x5 = ntk.create_pi();
    const auto x6 = ntk.create_pi();


    const auto n1  = ntk.create_not(x1);
    const auto n3  = ntk.create_not(x3);
    const auto a1  = ntk.create_and(n1, x2);
    const auto a2  = ntk.create_and(n3, x4);
    const auto a3  = ntk.create_and(a1, a2);
    const auto a4  = ntk.create_and(x5, x6);
    const auto a5  = ntk.create_and(a4, a3);

    ntk.create_po(a5);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> test_inv_flag_c()
{
    mockturtle::names_view<Ntk> ntk{};


    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();

    const auto x3 = ntk.create_pi();
    const auto x4 = ntk.create_pi();


    const auto n1  = ntk.create_not(x1);
    const auto n2  = ntk.create_not(x2);
    const auto n3  = ntk.create_not(x3);
    const auto a1  = ntk.create_and(n1, n2);
    const auto a2  = ntk.create_and(n3, x4);
    const auto a3  = ntk.create_and(a1, a2);

    ntk.create_po(a3);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> test_sort_inputs()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto z = ntk.create_pi();
    const auto a = ntk.create_pi();
    const auto b = ntk.create_pi();
    const auto c = ntk.create_pi();
    const auto d = ntk.create_pi();

    const auto f1 = ntk.create_and(a, b);
    const auto f2 = ntk.create_and(a, c);
    const auto f3 = ntk.create_and(f1, d);
    const auto f4 = ntk.create_and(f2, f3);
    const auto f5 = ntk.create_and(f4, z);

    ntk.create_po(f5);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> test_fanout_swap()
{
    mockturtle::names_view<Ntk> ntk{};


    const auto a = ntk.create_pi();//2
    const auto b = ntk.create_pi();//3
    const auto c = ntk.create_pi();//4


    const auto f1 = ntk.create_and(a, b);
    const auto n1 = ntk.create_not(a);
    const auto f2 = ntk.create_and(n1, c);
    const auto f3 = ntk.create_and(f1, f2);

    ntk.create_po(f3);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> test_sort_fanouts()
{
    mockturtle::names_view<Ntk> ntk{};

    //Not part of ordering, should be placed at the end
    const auto x = ntk.create_pi();//2

    //PI to PI should get ordered after FOs with 2PIs
    const auto y = ntk.create_pi();//3
    const auto z = ntk.create_pi();//4

    // FOs with 1PI should get ordered at the beginning
    const auto r = ntk.create_pi();//5
    const auto s = ntk.create_pi();//6

    //FOs with 2PIs should get ordered at the beginning
    const auto a = ntk.create_pi();//7
    const auto b = ntk.create_pi();//8
    const auto c = ntk.create_pi();//9
    const auto d = ntk.create_pi();//10
    const auto e = ntk.create_pi();//11
    const auto f = ntk.create_pi();//12


    const auto na = ntk.create_not(a);
    const auto f1 = ntk.create_and(na, b);
    const auto f3 = ntk.create_and(c, e);
    const auto f2 = ntk.create_and(a, d);
    const auto f4 = ntk.create_and(c, f);
    const auto f5 = ntk.create_and(f1, f3);
    const auto f6 = ntk.create_and(f2, f4);
    const auto f7 = ntk.create_and(f5, f6);
    const auto f8 = ntk.create_and(f7, x);
    const auto f9 = ntk.create_and(y, z);
    const auto f10 = ntk.create_and(f9, f8);
    const auto nr = ntk.create_not(r);
    const auto f11 = ntk.create_and(nr, s);
    const auto f12 = ntk.create_and(f10, r);
    const auto f13 = ntk.create_and(f11, f12);

    ntk.create_po(f13);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> test_fanout_nodes_rank()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto c = ntk.create_pi();//2
    const auto d = ntk.create_pi();//3
    const auto a = ntk.create_pi();//4
    const auto b = ntk.create_pi();//5



    const auto f1 = ntk.create_and(a, b);

    const auto f2 = ntk.create_and(c, d);
    const auto f3 = ntk.create_or(c, d);

    const auto f4 = ntk.create_and(f1, f2);
    const auto f5 = ntk.create_and(f3, f4);

    ntk.create_po(f5);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> test_fanout_nodes_coloring_null()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto a = ntk.create_pi();//2
    const auto b = ntk.create_pi();//3
    const auto c = ntk.create_pi();//4

    const auto f1 = ntk.create_and(a, b);
    const auto f2 = ntk.create_and(a, c);
    const auto f3 = ntk.create_and(b, c);

    const auto f4 = ntk.create_and(f1, f2);
    const auto f5 = ntk.create_and(f3, f4);

    ntk.create_po(f5);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_random_1()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto n_1 = ntk.create_pi("n_1");
    const auto n_2 = ntk.create_pi("n_2");
    const auto n_3 = ntk.create_pi("n_3");
    const auto n_4 = ntk.create_or(n_1, n_3);
    const auto n_5 = ntk.create_maj(n_1, n_2, n_4);
    const auto n_6 = ntk.create_or(n_3, n_5);
    const auto n_2i = ntk.create_not(n_2);
    const auto n_7 = ntk.create_maj(n_1, n_2i, n_6);
    const auto n_8 = ntk.create_maj(n_1, n_4, n_7);
    const auto n_9 = ntk.create_maj(n_2, n_5, n_6);
    const auto n_9i = ntk.create_not(n_9);
    const auto n_10 = ntk.create_maj(n_3, n_5, n_9i);
    const auto n_11 = ntk.create_and(n_2, n_10);
    const auto n_12 = ntk.create_and(n_2, n_7);
    const auto n_13 = ntk.create_or(n_11, n_12);
    ntk.create_po(n_8, "po0");
    ntk.create_po(n_13, "po1");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_one_buf()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto x1    = ntk.create_pi("a");
    const auto x2    = ntk.create_pi("b");
    const auto x3    = ntk.create_pi("c");
    const auto x4    = ntk.create_pi("d");

    const auto m1 = ntk.create_maj(x1, x2, x3);
    const auto a1 = ntk.create_and(m1, x4);

    ntk.create_po(a1, "f");

    return ntk;
}


template <typename Ntk>
mockturtle::names_view<Ntk> maj_one_buf_a()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1    = ntk.create_pi("a");
    const auto x2    = ntk.create_pi("b");
    const auto x3    = ntk.create_pi("c");
    const auto x0    = ntk.create_pi("0");
    const auto x00    = ntk.create_pi("00");
    const auto x4    = ntk.create_pi("d");

    const auto m1 = ntk.create_maj(x1, x2, x3);
    const auto a0 = ntk.create_and(x0, x00);
    const auto a2 = ntk.create_and(a0, x4);
    const auto a1 = ntk.create_and(m1, a2);

    ntk.create_po(a0, "f");
    ntk.create_po(a1, "f");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_two_buf_a()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto x1    = ntk.create_pi("a");
    const auto x2    = ntk.create_pi("b");
    const auto x3    = ntk.create_pi("c");

    const auto m1 = ntk.create_maj(x1, x2, x3);
    const auto a1 = ntk.create_and(x1, x2);
    const auto o1 = ntk.create_or(x2, x3);

    const auto a2 = ntk.create_and(m1, a1);
    const auto a3 = ntk.create_and(a1, o1);

    const auto o2 = ntk.create_or(a2, a3);

    ntk.create_po(o2, "f");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_two_buf_b()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto x1    = ntk.create_pi("a");
    const auto x2    = ntk.create_pi("b");
    const auto x3    = ntk.create_pi("c");
    const auto x4    = ntk.create_pi("d");

    const auto m1 = ntk.create_maj(x1, x2, x3);
    const auto a1 = ntk.create_or(x3, x4);

    const auto a2 = ntk.create_and(m1, a1);
    const auto a3 = ntk.create_and(x4, a2);

    ntk.create_po(a3, "f");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_two_buf()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto x1    = ntk.create_pi("a");
    const auto x2    = ntk.create_pi("b");
    const auto x3    = ntk.create_pi("c");
    const auto x4    = ntk.create_pi("c");
    const auto x5    = ntk.create_pi("c");

    const auto m1 = ntk.create_maj(x1, x2, x3);
    const auto a1 = ntk.create_and(m1, x4);
    const auto a2 = ntk.create_and(a1, x5);

    ntk.create_po(a2, "f");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_three_buf()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto x1    = ntk.create_pi("a");
    const auto x2    = ntk.create_pi("b");
    const auto x3    = ntk.create_pi("c");
    const auto x4    = ntk.create_pi("c");
    const auto x5    = ntk.create_pi("c");
    const auto x6    = ntk.create_pi("c");

    const auto m1 = ntk.create_maj(x1, x2, x3);
    const auto a1 = ntk.create_and(m1, x4);
    const auto a2 = ntk.create_and(a1, x5);
    const auto a3 = ntk.create_and(a2, x6);

    ntk.create_po(a3, "f");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_four_buf()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto x1    = ntk.create_pi("m1");
    const auto x2    = ntk.create_pi("m2");
    const auto x3    = ntk.create_pi("m3");
    const auto x4    = ntk.create_pi("d");
    const auto x5    = ntk.create_pi("e");
    const auto x6    = ntk.create_pi("f");
    const auto x7    = ntk.create_pi("g");

    const auto m1 = ntk.create_maj(x1, x2, x3);
    const auto a1 = ntk.create_and(m1, x4);
    const auto a2 = ntk.create_and(a1, x5);
    const auto a3 = ntk.create_and(a2, x6);
    const auto a4 = ntk.create_and(a3, x7);

    const auto m4 = ntk.create_maj(a1, x2, x3);

    ntk.create_po(a4, "f");

    ntk.create_po(m4, "f");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_e_s_buf()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto x1    = ntk.create_pi("a");
    const auto x2    = ntk.create_pi("b");
    const auto x3    = ntk.create_pi("c");
    const auto x4    = ntk.create_pi("d");
    const auto x5    = ntk.create_pi("e");
    const auto x6    = ntk.create_pi("f");

    const auto a1 = ntk.create_and(x4, x5);
    const auto m1 = ntk.create_maj(x1, x2, x3);
    const auto a2 = ntk.create_and(x6, m1);
    const auto a3 = ntk.create_and(a1, a2);

    ntk.create_po(a3, "f");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_mg_buf()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto x1    = ntk.create_pi("a");
    const auto x2    = ntk.create_pi("b");
    const auto x3    = ntk.create_pi("c");

    const auto m1 = ntk.create_maj(x1, x2, x3);

    const auto a1 = ntk.create_and(x1, x2);
    const auto a2 = ntk.create_and(x2, x3);

    const auto m2 = ntk.create_maj(m1, a1, a2);

    ntk.create_po(m2, "f");

    ntk.create_po(a1, "m");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> se_coloring_corner_case_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("x1");
    const auto x2 = ntk.create_pi("x2");

    const auto x3 = ntk.create_buf(x1);
    const auto x4 = ntk.create_buf(x2);

    const auto x5 = ntk.create_and(x3, x4);

    const auto x6 = ntk.create_buf(x5);

    const auto x7 = ntk.create_and(x3, x6);
    const auto x8 = ntk.create_and(x4, x6);

    ntk.create_po(x7, "f1");
    ntk.create_po(x8, "f2");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> fanout_substitution_corner_case_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("x1");

    const auto fo1 = ntk.create_buf(x1);
    const auto fo2 = ntk.create_buf(fo1);
    const auto fo3 = ntk.create_buf(fo1);

    const auto n1 = ntk.create_not(fo2);
    const auto n2 = ntk.create_not(fo2);
    const auto n3 = ntk.create_not(fo3);
    const auto n4 = ntk.create_not(fo3);

    ntk.create_po(n1, "f1");
    ntk.create_po(n2, "f2");
    ntk.create_po(n3, "f3");
    ntk.create_po(n4, "f4");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> inverter_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("x1");

    const auto fo1 = ntk.create_buf(x1);

    const auto n1 = ntk.create_not(fo1);
    const auto n2 = ntk.create_not(fo1);

    ntk.create_po(n1, "f1");
    ntk.create_po(n2, "f2");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> clpl()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("x1");
    const auto x2 = ntk.create_pi("x2");
    const auto x3 = ntk.create_pi("x3");
    const auto x4 = ntk.create_pi("x4");
    const auto x5 = ntk.create_pi("x5");
    //    const auto x6 = ntk.create_pi("x6");
    //    const auto x7 = ntk.create_pi("x7");
    //    const auto x8 = ntk.create_pi("x8");
    //    const auto x9 = ntk.create_pi("x9");
    //    const auto x10 = ntk.create_pi("x10");
    //    const auto x11 = ntk.create_pi("x11");

    const auto a1 = ntk.create_and(x1, x2);
    const auto o1 = ntk.create_or(a1, x3);
    const auto a2 = ntk.create_and(o1, x4);
    const auto o2 = ntk.create_or(a2, x5);
    //    const auto a3 = ntk.create_and(o2, x6);
    //    const auto o3 = ntk.create_or(a3, x7);
    //    const auto a4 = ntk.create_and(o3, x8);
    //    const auto o4 = ntk.create_or(a4, x9);
    //    const auto a5 = ntk.create_and(o4, x10);
    //    const auto o5 = ntk.create_or(a5, x11);

    ntk.create_po(o1, "f1");
    ntk.create_po(o2, "f2");
    //    ntk.create_po(o3, "f3");
    //    ntk.create_po(o4, "f4");
    //    ntk.create_po(o5, "f5");

    return ntk;
}

template <typename Ntk>
Ntk one_to_five_path_difference_network()
{
    Ntk ntk{};

    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();

    const auto p1 = ntk.create_buf(x1);
    const auto p2 = ntk.create_buf(ntk.create_buf(ntk.create_buf(ntk.create_buf(ntk.create_buf(x2)))));

    const auto a = ntk.create_and(p1, p2);

    ntk.create_po(a);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> nand_xnor_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("x1");
    const auto x2 = ntk.create_pi("x2");

    const auto nand1 = ntk.create_nand(x1, x2);
    const auto nor1  = ntk.create_nor(x1, x2);
    const auto xor1  = ntk.create_xor(nand1, nor1);
    const auto xnor1 = ntk.create_xnor(nand1, nor1);

    ntk.create_po(xor1, "f1");
    ntk.create_po(xnor1, "f2");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> topolinano_network()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("x1");
    const auto x2 = ntk.create_pi("x2");

    const auto fo1 = ntk.create_buf(x1);
    const auto fo2 = ntk.create_buf(fo1);
    const auto a1  = ntk.create_and(x1, x2);
    const auto o1  = ntk.create_or(a1, fo2);

    ntk.create_po(fo1, "f1");
    ntk.create_po(fo2, "f2");
    ntk.create_po(o1, "f3");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> seq_one()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("a");

    const auto r1_o = ntk.create_ro();

    const auto xo1 = ntk.create_xor(x1, r1_o);

    ntk.create_ri(xo1);

    ntk.create_po(xo1, "cout");


    return ntk;
}
template <typename Ntk>
mockturtle::names_view<Ntk> seq_two()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("a");

    const auto r1_o = ntk.create_ro();

    const auto r2_o = ntk.create_ro();

    const auto xo1 = ntk.create_xor(x1, r1_o);

    const auto a1 = ntk.create_and(r1_o, r2_o);

    ntk.create_po(a1, "cout");
    ntk.create_ri(xo1);
    ntk.create_ri(a1);

    return ntk;
}
template <typename Ntk>
mockturtle::names_view<Ntk> seq_three()
{
    mockturtle::names_view<Ntk> ntk{};

    const auto x1 = ntk.create_pi("a");

    const auto r1_o = ntk.create_ro();

    const auto r2_o = ntk.create_ro();

    const auto r3_o = ntk.create_ro();

    const auto xo1 = ntk.create_xor(x1, r1_o);

    const auto a1 = ntk.create_and(x1, r2_o);

    const auto a2 = ntk.create_and(a1, r3_o);

    ntk.create_po(a2, "cout");
    ntk.create_ri(xo1);
    ntk.create_ri(a1);
    ntk.create_ri(a2);

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> par_check()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto a = ntk.create_pi("a");
    const auto b = ntk.create_pi("b");
    const auto c = ntk.create_pi("c");
    const auto p = ntk.create_pi("p");
    const auto w1 = ntk.create_not(b);
    const auto w2 = ntk.create_and(a, w1);
    const auto w3 = ntk.create_not(a);
    const auto w4 = ntk.create_and(w3, b);
    const auto n1 = ntk.create_or(w2, w4);
    const auto w5 = ntk.create_not(p);
    const auto w6 = ntk.create_and(w5, c);
    const auto w7 = ntk.create_not(a);
    const auto w8 = ntk.create_and(w7, p);
    const auto n2 = ntk.create_or(w6, w8);
    const auto w9 = ntk.create_not(n2);
    const auto w10 = ntk.create_and(n1, w9);
    const auto w11 = ntk.create_not(n1);
    const auto w12 = ntk.create_and(w11, n2);
    const auto parity_check = ntk.create_or(w10, w12);

    ntk.create_po(parity_check, "parity_check");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_random_2()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto n_1 = ntk.create_pi("n_1");
    const auto n_2 = ntk.create_pi("n_2");
    const auto n_3 = ntk.create_pi("n_3");
    const auto n_4 = ntk.create_pi("n_4");
    const auto n_4i = ntk.create_not(n_4);
    const auto n_5 = ntk.create_maj(n_2, n_3, n_4i);
    const auto n_5i = ntk.create_not(n_5);
    const auto n_6 = ntk.create_maj(n_1, n_2, n_5i);
    const auto n_7 = ntk.create_and(n_2, n_3);
    const auto n_7i = ntk.create_not(n_7);
    const auto n_8 = ntk.create_maj(n_1, n_5, n_7i);
    const auto n_8i = ntk.create_not(n_8);
    const auto n_9 = ntk.create_and(n_7, n_8i);
    const auto n_10 = ntk.create_maj(n_2, n_3, n_8);
    const auto n_11 = ntk.create_maj(n_1, n_5i, n_10);
    const auto n_12 = ntk.create_maj(n_4, n_9, n_11);
    /*const auto n_13 = ntk.create_maj(n_1, n_5i, n_8);
    const auto n_14 = ntk.create_maj(n_1, n_4, n_13);*/
    ntk.create_po(n_6, "po0");
    ntk.create_po(n_12, "po1");
    //ntk.create_po(n_14, "po2");

    return ntk;
}

template <typename Ntk>
mockturtle::names_view<Ntk> maj_random_5()
{
    mockturtle::names_view<Ntk> ntk{};
    const auto n_1 = ntk.create_pi("n_1");
    const auto n_2 = ntk.create_pi("n_2");
    const auto n_3 = ntk.create_pi("n_3");
    const auto n_4 = ntk.create_pi("n_4");
    const auto n_5 = ntk.create_pi("n_5");
    const auto n_6 = ntk.create_pi("n_6");
    const auto n_7 = ntk.create_pi("n_7");
    const auto n_8 = ntk.create_pi("n_8");
    const auto n_9 = ntk.create_pi("n_9");
    const auto n_10 = ntk.create_pi("n_10");
    const auto n_10i = ntk.create_not(n_10);
    const auto n_11 = ntk.create_maj(n_3, n_8, n_10i);
    const auto n_11i = ntk.create_not(n_11);
    const auto n_12 = ntk.create_maj(n_1, n_5, n_11i);
    const auto n_13 = ntk.create_maj(n_2, n_11, n_12);
    const auto n_14 = ntk.create_maj(n_2, n_6, n_9);
    const auto n_15 = ntk.create_maj(n_11, n_12, n_14);
    const auto n_16 = ntk.create_maj(n_3, n_5, n_15);
    const auto n_17 = ntk.create_and(n_2, n_9);
    const auto n_14i = ntk.create_not(n_14);
    const auto n_18 = ntk.create_maj(n_11, n_14i, n_15);
    const auto n_19 = ntk.create_and(n_7, n_18);
    const auto n_19i = ntk.create_not(n_19);
    const auto n_20 = ntk.create_maj(n_1, n_17, n_19i);
    const auto n_16i = ntk.create_not(n_16);
    const auto n_21 = ntk.create_maj(n_13, n_16i, n_20);
    const auto n_6i = ntk.create_not(n_6);
    const auto n_22 = ntk.create_maj(n_4, n_5, n_6i);
    const auto n_23 = ntk.create_maj(n_11, n_20, n_22);
    const auto n_22i = ntk.create_not(n_22);
    const auto n_24 = ntk.create_and(n_6, n_22i);
    const auto n_24i = ntk.create_not(n_24);
    const auto n_25 = ntk.create_maj(n_21, n_23, n_24i);
    const auto n_25i = ntk.create_not(n_25);
    const auto n_26 = ntk.create_maj(n_4, n_15, n_25i);
    const auto n_27 = ntk.create_maj(n_8, n_21, n_26);
    const auto n_28 = ntk.create_maj(n_5, n_10, n_23);
    const auto n_29 = ntk.create_maj(n_5, n_8, n_24);
    const auto n_28i = ntk.create_not(n_28);
    const auto n_30 = ntk.create_maj(n_10, n_28i, n_29);
    const auto n_31 = ntk.create_maj(n_1, n_27, n_30);
    const auto n_32 = ntk.create_maj(n_5, n_10, n_22i);
    const auto n_32i = ntk.create_not(n_32);
    const auto n_33 = ntk.create_maj(n_11, n_16, n_32i);
    const auto n_34 = ntk.create_maj(n_5, n_15, n_33);
    const auto n_35 = ntk.create_maj(n_2, n_16, n_19);
    const auto n_36 = ntk.create_and(n_1, n_8);
    const auto n_37 = ntk.create_maj(n_11, n_21, n_36);
    const auto n_38 = ntk.create_maj(n_6, n_7, n_36);
    const auto n_38i = ntk.create_not(n_38);
    const auto n_39 = ntk.create_maj(n_28, n_37, n_38i);
    const auto n_40 = ntk.create_and(n_20, n_39);
    const auto n_40i = ntk.create_not(n_40);
    const auto n_41 = ntk.create_maj(n_27, n_35, n_40i);
    const auto n_9i = ntk.create_not(n_9);
    const auto n_42 = ntk.create_maj(n_6, n_7, n_9i);
    const auto n_43 = ntk.create_maj(n_4, n_15, n_42);
    const auto n_43i = ntk.create_not(n_43);
    const auto n_44 = ntk.create_maj(n_15, n_18, n_43i);
    const auto n_44i = ntk.create_not(n_44);
    const auto n_45 = ntk.create_and(n_13, n_44i);
    const auto n_46 = ntk.create_maj(n_3, n_5, n_14);
    const auto n_45i = ntk.create_not(n_45);
    const auto n_47 = ntk.create_maj(n_7, n_45i, n_46);
    const auto n_37i = ntk.create_not(n_37);
    const auto n_48 = ntk.create_maj(n_7, n_19, n_37i);
    const auto n_49 = ntk.create_maj(n_12, n_15, n_43);
    const auto n_50 = ntk.create_maj(n_4, n_9i, n_17);
    const auto n_51 = ntk.create_maj(n_1, n_43, n_50);
    const auto n_21i = ntk.create_not(n_21);
    const auto n_52 = ntk.create_maj(n_6, n_17, n_21i);
    const auto n_52i = ntk.create_not(n_52);
    const auto n_53 = ntk.create_maj(n_14, n_51, n_52i);
    const auto n_54 = ntk.create_maj(n_17, n_42, n_53);
    const auto n_54i = ntk.create_not(n_54);
    const auto n_55 = ntk.create_maj(n_15, n_49, n_54i);
    const auto n_56 = ntk.create_maj(n_12, n_48, n_55);
    const auto n_17i = ntk.create_not(n_17);
    const auto n_57 = ntk.create_maj(n_5, n_17i, n_22);
    const auto n_58 = ntk.create_maj(n_1, n_28, n_57);
    const auto n_59 = ntk.create_and(n_5, n_8);
    const auto n_60 = ntk.create_maj(n_10, n_17i, n_59);
    const auto n_60i = ntk.create_not(n_60);
    const auto n_61 = ntk.create_maj(n_28, n_45, n_60i);
    const auto n_62 = ntk.create_maj(n_3, n_17, n_61);
    const auto n_62i = ntk.create_not(n_62);
    const auto n_63 = ntk.create_maj(n_12, n_48, n_62i);
    const auto n_64 = ntk.create_maj(n_20, n_32, n_44i);
    const auto n_39i = ntk.create_not(n_39);
    const auto n_65 = ntk.create_maj(n_29, n_39i, n_64);
    const auto n_66 = ntk.create_maj(n_17, n_28, n_32i);
    const auto n_66i = ntk.create_not(n_66);
    const auto n_67 = ntk.create_maj(n_23, n_49, n_66i);
    const auto n_68 = ntk.create_maj(n_20, n_22, n_67);
    const auto n_68i = ntk.create_not(n_68);
    const auto n_69 = ntk.create_maj(n_2, n_65, n_68i);
    const auto n_63i = ntk.create_not(n_63);
    const auto n_70 = ntk.create_maj(n_58, n_63i, n_69);
    const auto n_13i = ntk.create_not(n_13);
    const auto n_71 = ntk.create_maj(n_3, n_9, n_13i);
    const auto n_72 = ntk.create_maj(n_14, n_60i, n_71);
    const auto n_15i = ntk.create_not(n_15);
    const auto n_73 = ntk.create_maj(n_8, n_12, n_15i);
    const auto n_74 = ntk.create_maj(n_19, n_50, n_73);
    const auto n_74i = ntk.create_not(n_74);
    const auto n_75 = ntk.create_maj(n_29, n_45, n_74i);
    const auto n_75i = ntk.create_not(n_75);
    const auto n_76 = ntk.create_maj(n_27, n_66, n_75i);
    const auto n_76i = ntk.create_not(n_76);
    const auto n_77 = ntk.create_maj(n_24, n_35, n_76i);
    const auto n_78 = ntk.create_and(n_4, n_62);
    const auto n_79 = ntk.create_maj(n_5, n_17, n_78);
    const auto n_80 = ntk.create_maj(n_45, n_77, n_79);
    const auto n_81 = ntk.create_maj(n_27, n_72, n_80);
    const auto n_82 = ntk.create_maj(n_3, n_19, n_30);
    const auto n_83 = ntk.create_maj(n_67, n_76i, n_82);
    const auto n_84 = ntk.create_maj(n_3, n_54i, n_83);
    const auto n_85 = ntk.create_maj(n_12, n_22i, n_44);
    const auto n_86 = ntk.create_maj(n_2, n_11, n_21);
    const auto n_87 = ntk.create_maj(n_42, n_73, n_86);
    const auto n_87i = ntk.create_not(n_87);
    const auto n_88 = ntk.create_maj(n_14, n_33, n_87i);
    const auto n_88i = ntk.create_not(n_88);
    const auto n_89 = ntk.create_maj(n_25, n_75, n_88i);
    const auto n_64i = ntk.create_not(n_64);
    const auto n_90 = ntk.create_maj(n_7, n_64i, n_89);
    const auto n_86i = ntk.create_not(n_86);
    const auto n_91 = ntk.create_maj(n_2, n_20, n_86i);
    const auto n_92 = ntk.create_maj(n_86, n_88i, n_91);
    const auto n_93 = ntk.create_maj(n_5, n_8, n_22);
    const auto n_89i = ntk.create_not(n_89);
    const auto n_94 = ntk.create_maj(n_24, n_89i, n_93);
    const auto n_95 = ntk.create_maj(n_17, n_25i, n_36);
    const auto n_95i = ntk.create_not(n_95);
    const auto n_96 = ntk.create_maj(n_55, n_89, n_95i);
    const auto n_58i = ntk.create_not(n_58);
    const auto n_97 = ntk.create_maj(n_15, n_58i, n_96);
    const auto n_98 = ntk.create_maj(n_6, n_13, n_61);
    const auto n_98i = ntk.create_not(n_98);
    const auto n_99 = ntk.create_maj(n_24, n_32, n_98i);
    const auto n_20i = ntk.create_not(n_20);
    const auto n_100 = ntk.create_maj(n_8, n_20i, n_99);
    const auto n_101 = ntk.create_maj(n_42, n_62, n_63);
    const auto n_101i = ntk.create_not(n_101);
    const auto n_102 = ntk.create_maj(n_38, n_75, n_101i);
    const auto n_103 = ntk.create_maj(n_9, n_20i, n_87);
    const auto n_18i = ntk.create_not(n_18);
    const auto n_104 = ntk.create_maj(n_11, n_18i, n_59);
    const auto n_105 = ntk.create_maj(n_3, n_35, n_104);
    const auto n_106 = ntk.create_maj(n_37, n_38, n_105);
    const auto n_107 = ntk.create_maj(n_55, n_104, n_106);
    const auto n_108 = ntk.create_maj(n_44, n_103, n_107);
    const auto n_109 = ntk.create_maj(n_18, n_60i, n_74);
    const auto n_110 = ntk.create_maj(n_60, n_98, n_109);
    const auto n_111 = ntk.create_maj(n_4, n_19, n_44);
    const auto n_111i = ntk.create_not(n_111);
    const auto n_112 = ntk.create_maj(n_7, n_32, n_111i);
    const auto n_113 = ntk.create_maj(n_74, n_110, n_112);
    const auto n_114 = ntk.create_maj(n_10, n_15, n_62i);
    const auto n_115 = ntk.create_maj(n_58, n_86i, n_114);
    const auto n_116 = ntk.create_maj(n_57, n_91, n_115);
    const auto n_117 = ntk.create_maj(n_20, n_58, n_116);
    const auto n_118 = ntk.create_maj(n_16, n_35, n_44);
    const auto n_119 = ntk.create_maj(n_44, n_64, n_75);
    const auto n_120 = ntk.create_maj(n_17, n_57, n_64i);
    const auto n_121 = ntk.create_maj(n_1, n_22i, n_120);
    const auto n_122 = ntk.create_maj(n_12, n_50, n_121);
    const auto n_123 = ntk.create_maj(n_105, n_109, n_122);
    const auto n_119i = ntk.create_not(n_119);
    const auto n_124 = ntk.create_maj(n_118, n_119i, n_123);
    const auto n_23i = ntk.create_not(n_23);
    const auto n_125 = ntk.create_maj(n_17, n_23i, n_62);
    const auto n_126 = ntk.create_maj(n_59, n_60, n_125);
    const auto n_127 = ntk.create_maj(n_2, n_82, n_93);
    const auto n_128 = ntk.create_maj(n_28, n_49, n_93);
    const auto n_120i = ntk.create_not(n_120);
    const auto n_129 = ntk.create_maj(n_39, n_120i, n_128);
    const auto n_130 = ntk.create_maj(n_61, n_127, n_129);
    const auto n_126i = ntk.create_not(n_126);
    const auto n_131 = ntk.create_maj(n_35, n_126i, n_130);
    const auto n_127i = ntk.create_not(n_127);
    const auto n_132 = ntk.create_maj(n_48, n_98, n_127i);
    const auto n_133 = ntk.create_maj(n_38, n_44, n_75);
    const auto n_134 = ntk.create_maj(n_26, n_54i, n_121);
    const auto n_134i = ntk.create_not(n_134);
    const auto n_135 = ntk.create_maj(n_3, n_123, n_134i);
    const auto n_83i = ntk.create_not(n_83);
    const auto n_136 = ntk.create_maj(n_32, n_65, n_83i);
    const auto n_99i = ntk.create_not(n_99);
    const auto n_137 = ntk.create_maj(n_10, n_69, n_99i);
    const auto n_138 = ntk.create_maj(n_44, n_136, n_137);
    const auto n_138i = ntk.create_not(n_138);
    const auto n_139 = ntk.create_maj(n_48, n_74, n_138i);
    const auto n_140 = ntk.create_maj(n_25, n_127, n_139);
    const auto n_141 = ntk.create_and(n_37, n_72);
    const auto n_142 = ntk.create_maj(n_25, n_27, n_141);
    const auto n_142i = ntk.create_not(n_142);
    const auto n_143 = ntk.create_maj(n_23, n_60, n_142i);
    const auto n_144 = ntk.create_maj(n_11, n_21, n_73);
    const auto n_145 = ntk.create_maj(n_4, n_8, n_77);
    const auto n_71i = ntk.create_not(n_71);
    const auto n_146 = ntk.create_maj(n_66, n_71i, n_95);
    const auto n_147 = ntk.create_maj(n_144, n_145, n_146);
    const auto n_78i = ntk.create_not(n_78);
    const auto n_148 = ntk.create_maj(n_9, n_78i, n_134);
    const auto n_27i = ntk.create_not(n_27);
    const auto n_149 = ntk.create_maj(n_3, n_27i, n_141);
    const auto n_150 = ntk.create_maj(n_3, n_141, n_149);
    const auto n_151 = ntk.create_maj(n_28, n_60, n_128);
    const auto n_152 = ntk.create_maj(n_21, n_75, n_151);
    const auto n_153 = ntk.create_maj(n_71, n_86, n_93);
    const auto n_154 = ntk.create_maj(n_73, n_110, n_153);
    const auto n_149i = ntk.create_not(n_149);
    const auto n_155 = ntk.create_maj(n_18, n_149i, n_154);
    const auto n_152i = ntk.create_not(n_152);
    const auto n_156 = ntk.create_maj(n_63, n_152i, n_155);
    const auto n_157 = ntk.create_maj(n_11, n_116, n_151);
    const auto n_158 = ntk.create_maj(n_24, n_61, n_62);
    const auto n_159 = ntk.create_maj(n_129, n_130, n_158);
    const auto n_159i = ntk.create_not(n_159);
    const auto n_160 = ntk.create_maj(n_57, n_83, n_159i);
    const auto n_161 = ntk.create_maj(n_8, n_15, n_63);
    const auto n_55i = ntk.create_not(n_55);
    const auto n_162 = ntk.create_maj(n_18, n_55i, n_65);
    const auto n_163 = ntk.create_maj(n_27, n_76i, n_162);
    const auto n_163i = ntk.create_not(n_163);
    const auto n_164 = ntk.create_maj(n_6, n_77, n_163i);
    const auto n_165 = ntk.create_maj(n_14, n_39, n_46);
    const auto n_166 = ntk.create_maj(n_2, n_29, n_165);
    const auto n_167 = ntk.create_maj(n_1, n_58, n_109);
    const auto n_168 = ntk.create_maj(n_39, n_45, n_57);
    const auto n_167i = ntk.create_not(n_167);
    const auto n_169 = ntk.create_maj(n_39, n_167i, n_168);
    const auto n_116i = ntk.create_not(n_116);
    const auto n_170 = ntk.create_maj(n_30, n_63, n_116i);
    const auto n_170i = ntk.create_not(n_170);
    const auto n_171 = ntk.create_maj(n_58, n_169, n_170i);
    const auto n_115i = ntk.create_not(n_115);
    const auto n_172 = ntk.create_maj(n_67, n_115i, n_171);
    const auto n_93i = ntk.create_not(n_93);
    const auto n_173 = ntk.create_maj(n_78, n_89, n_93i);
    const auto n_128i = ntk.create_not(n_128);
    const auto n_174 = ntk.create_maj(n_28, n_128i, n_173);
    const auto n_175 = ntk.create_maj(n_25, n_51, n_64i);
    const auto n_176 = ntk.create_maj(n_6, n_95, n_175);
    const auto n_177 = ntk.create_maj(n_20, n_79, n_176);
    const auto n_174i = ntk.create_not(n_174);
    const auto n_178 = ntk.create_maj(n_173, n_174i, n_177);
    const auto n_125i = ntk.create_not(n_125);
    const auto n_179 = ntk.create_maj(n_23, n_29, n_125i);
    const auto n_179i = ntk.create_not(n_179);
    const auto n_180 = ntk.create_maj(n_45, n_61, n_179i);
    const auto n_80i = ntk.create_not(n_80);
    const auto n_181 = ntk.create_maj(n_59, n_80i, n_180);
    const auto n_182 = ntk.create_maj(n_69, n_71, n_181);
    const auto n_183 = ntk.create_maj(n_61, n_112, n_118);
    const auto n_184 = ntk.create_or(n_19, n_36);
    const auto n_185 = ntk.create_maj(n_12, n_42, n_144);
    const auto n_112i = ntk.create_not(n_112);
    const auto n_186 = ntk.create_maj(n_8, n_112i, n_123);
    const auto n_187 = ntk.create_maj(n_22, n_27, n_39);
    const auto n_188 = ntk.create_maj(n_5, n_112, n_187);
    const auto n_189 = ntk.create_maj(n_136, n_155, n_188);
    const auto n_190 = ntk.create_maj(n_88, n_95, n_189);
    const auto n_191 = ntk.create_maj(n_37, n_48, n_98i);
    const auto n_191i = ntk.create_not(n_191);
    const auto n_192 = ntk.create_maj(n_62, n_176, n_191i);
    const auto n_193 = ntk.create_maj(n_12, n_105, n_192);
    const auto n_105i = ntk.create_not(n_105);
    const auto n_194 = ntk.create_maj(n_86, n_105i, n_120);
    const auto n_195 = ntk.create_maj(n_36, n_72, n_75i);
    const auto n_196 = ntk.create_maj(n_107, n_129, n_195);
    const auto n_197 = ntk.create_maj(n_155, n_194, n_196);
    const auto n_198 = ntk.create_maj(n_18, n_86, n_88);
    const auto n_198i = ntk.create_not(n_198);
    const auto n_199 = ntk.create_maj(n_71, n_73, n_198i);
    const auto n_199i = ntk.create_not(n_199);
    const auto n_200 = ntk.create_maj(n_11, n_93, n_199i);
    const auto n_201 = ntk.create_maj(n_10, n_17, n_125);
    const auto n_171i = ntk.create_not(n_171);
    const auto n_202 = ntk.create_maj(n_76, n_168, n_171i);
    const auto n_121i = ntk.create_not(n_121);
    const auto n_203 = ntk.create_maj(n_44, n_73, n_121i);
    const auto n_204 = ntk.create_maj(n_74, n_170, n_176);
    const auto n_205 = ntk.create_maj(n_50, n_203, n_204);
    const auto n_50i = ntk.create_not(n_50);
    const auto n_206 = ntk.create_maj(n_48, n_50i, n_154);
    const auto n_207 = ntk.create_maj(n_115, n_129, n_188);
    const auto n_208 = ntk.create_maj(n_54, n_119, n_207);
    const auto n_209 = ntk.create_maj(n_60, n_68, n_208);
    const auto n_210 = ntk.create_maj(n_6, n_12, n_171);
    ntk.create_po(n_31, "po0");
    ntk.create_po(n_34, "po1");
    ntk.create_po(n_41, "po2");
    ntk.create_po(n_47, "po3");
    ntk.create_po(n_56, "po4");
    ntk.create_po(n_70, "po5");
    ntk.create_po(n_81, "po6");
    ntk.create_po(n_84, "po7");
    ntk.create_po(n_85, "po8");
    ntk.create_po(n_90, "po9");
    ntk.create_po(n_92, "po10");
    ntk.create_po(n_94, "po11");
    ntk.create_po(n_97, "po12");
    ntk.create_po(n_100, "po13");
    ntk.create_po(n_102, "po14");
    ntk.create_po(n_108, "po15");
    ntk.create_po(n_113, "po16");
    ntk.create_po(n_117, "po17");
    ntk.create_po(n_124, "po18");
    ntk.create_po(n_131, "po19");
    ntk.create_po(n_132, "po20");
    ntk.create_po(n_133, "po21");
    ntk.create_po(n_135, "po22");
    ntk.create_po(n_140, "po23");
    ntk.create_po(n_143, "po24");
    ntk.create_po(n_147, "po25");
    ntk.create_po(n_148, "po26");
    ntk.create_po(n_150, "po27");
    ntk.create_po(n_156, "po28");
    ntk.create_po(n_157, "po29");
    ntk.create_po(n_160, "po30");
    ntk.create_po(n_161, "po31");
    ntk.create_po(n_164, "po32");
    ntk.create_po(n_166, "po33");
    ntk.create_po(n_172, "po34");
    ntk.create_po(n_178, "po35");
    ntk.create_po(n_182, "po36");
    ntk.create_po(n_183, "po37");
    ntk.create_po(n_184, "po38");
    ntk.create_po(n_185, "po39");
    ntk.create_po(n_186, "po40");
    ntk.create_po(n_190, "po41");
    ntk.create_po(n_193, "po42");
    ntk.create_po(n_197, "po43");
    ntk.create_po(n_200, "po44");
    ntk.create_po(n_201, "po45");
    ntk.create_po(n_202, "po46");
    ntk.create_po(n_205, "po47");
    ntk.create_po(n_206, "po48");
    ntk.create_po(n_209, "po49");
    ntk.create_po(n_210, "po50");

    return ntk;
}

}  // namespace blueprints

#endif  // FICTION_NETWORK_BLUEPRINTS_HPP
