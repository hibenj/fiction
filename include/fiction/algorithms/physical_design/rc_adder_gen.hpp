//
// Created by benjamin on 13.05.25.
//

#ifndef FICTION_RC_ADDER_GEN_HPP
#define FICTION_RC_ADDER_GEN_HPP

namespace fiction
{

namespace detail
{
template <typename Lyt>
class rc_adder_gen_impl
{
  public:
    rc_adder_gen_impl(uint64_t num_bits) : num_bits{num_bits} {}

    Lyt run()
    {
        num_clks number_of_clock_phases = num_clks::FOUR;
        uint64_t x_ratio = (8 * num_bits) - 2;
        uint64_t y_ratio = (3 * num_bits) + 1;
        Lyt      lyt{{x_ratio, y_ratio}, twoddwave_clocking<Lyt>(number_of_clock_phases)};

        uint64_t signal_a = 0;
        uint64_t signal_b = 0;

        // Create carry-in input at the top
        tile<Lyt> carry_pos{0, 2 * num_bits};
        uint64_t signal_c = lyt.create_pi("Cin", carry_pos);

        // Create primary inputs for each bit
        for (uint32_t i = 0; i < num_bits; ++i)
        {
            const uint32_t y_base = 2 * (num_bits - 1 - i);

            tile<Lyt> a_pos{0, y_base};
            tile<Lyt> b_pos{0, y_base + 1};

            signal_a = lyt.create_pi("a" + std::to_string(i), a_pos);
            signal_b = lyt.create_pi("b" + std::to_string(i), b_pos);

            // Route inputs to starting positions of the FA structure
            if (i != 0)
            {
                uint32_t x_base = (8 * i) - 1;

                // create helper tiles
                tile<Lyt> a_wire_target{x_base + 1, a_pos.y};
                tile<Lyt> b_wire_target{x_base, b_pos.y};

                // wire east
                wire_east(lyt, a_pos, {a_wire_target.x + 1, a_wire_target.y});
                wire_east(lyt, b_pos, {b_wire_target.x + 1, b_wire_target.y});

                // update positions
                a_pos = a_wire_target;
                b_pos = b_wire_target;

                // update helper
                a_wire_target = {a_pos.x, a_pos.y + (i * 3)};
                b_wire_target = {b_pos.x, b_pos.y + (i * 3)};

                // wire south
                signal_a = wire_south(lyt, a_pos, {a_wire_target.x, a_wire_target.y + 1});
                wire_south(lyt, b_pos, {b_wire_target.x, b_wire_target.y + 1});
                b_pos         = b_wire_target;
                b_wire_target = {b_pos.x + 1, b_pos.y};
                signal_b      = wire_east(lyt, b_pos, {b_wire_target.x + 1, b_wire_target.y});

                // update positions
                a_pos = a_wire_target;
                b_pos = b_wire_target;
            }
            // insert the FA structure
            carry_pos = insert_fa(lyt, a_pos, signal_a, signal_b, signal_c);
            if (i != num_bits - 1)
            {
                signal_c = wire_east(lyt, carry_pos, {carry_pos.x + 3, carry_pos.y});
            }
        }

        for (int i = 0; i < pos.size(); ++i)
        {
            tile<Lyt> po_tile = { (i * 8) + 2, y_ratio};
            lyt.create_po(wire_south(lyt, pos_pos[i], po_tile),
                                 fmt::format("sum{}", i), po_tile);
        }

        tile<Lyt> po_tile = {x_ratio, y_ratio};
        lyt.create_po(wire_south(lyt, {x_ratio, y_ratio - 1}, po_tile),
                      "Cout", po_tile);

        return lyt;
    }

  private:
    uint64_t num_bits = 0;
    std::vector<uint64_t> pos{};
    std::vector<tile<Lyt>> pos_pos{};

    tile<Lyt> insert_fa(Lyt lyt, tile<Lyt> a_pos, uint64_t signal_a, uint64_t signal_b, uint64_t signal_c)
    {
        //
        const auto s01 = lyt.create_buf(signal_a, {a_pos.x + 1, a_pos.y});
        const auto s02 = lyt.create_buf(s01, {a_pos.x + 2, a_pos.y});
        const auto s03 = lyt.create_buf(s02, {a_pos.x + 3, a_pos.y});
        const auto s04 = lyt.create_buf(s03, {a_pos.x + 4, a_pos.y});
        const auto s05 = lyt.create_buf(s04, {a_pos.x + 5, a_pos.y});
        const auto s06 = lyt.create_buf(s05, {a_pos.x + 6, a_pos.y});
        //
        const auto s11 = lyt.create_xor(signal_b, s01, {a_pos.x + 1, a_pos.y + 1});
        const auto s12 = lyt.create_buf(s02, {a_pos.x + 2, a_pos.y + 1});
        const auto s13 = lyt.create_buf(s12, {a_pos.x + 3, a_pos.y + 1});
        const auto s14 = lyt.create_buf(s13, {a_pos.x + 4, a_pos.y + 1});
        const auto s15 = lyt.create_buf(s14, {a_pos.x + 5, a_pos.y + 1});
        const auto s16 = lyt.create_buf(s06, {a_pos.x + 6, a_pos.y + 1});
        //
        const auto s21 = lyt.create_buf(s11, {a_pos.x + 1, a_pos.y + 2});
        const auto s22 = lyt.create_buf(s21, {a_pos.x + 2, a_pos.y + 2});
        const auto s23 = lyt.create_buf(s22, {a_pos.x + 3, a_pos.y + 2});
        const auto s24 = lyt.create_buf(s23, {a_pos.x + 4, a_pos.y + 2});
        const auto s25 = lyt.create_xor(s15, s24, {a_pos.x + 5, a_pos.y + 2});
        const auto s26 = lyt.create_and(s16, s25, {a_pos.x + 6, a_pos.y + 2});
        //
        const auto s30 = lyt.create_buf(signal_c, {a_pos.x, a_pos.y + 3});
        const auto s31 = lyt.create_xor(s21, s30, {a_pos.x + 1, a_pos.y + 3});
        const auto s32 = lyt.create_buf(s31, {a_pos.x + 2, a_pos.y + 3});
        const auto s33 = lyt.create_xor(s23, s32, {a_pos.x + 3, a_pos.y + 3});
        const auto s34 = lyt.create_and(s24, s33, {a_pos.x + 4, a_pos.y + 3});
        const auto s35 = lyt.create_buf(s34, {a_pos.x + 5, a_pos.y + 3});
        lyt.create_or(s26, s35, {a_pos.x + 6, a_pos.y + 3});

        pos.push_back(s32);
        tile<Lyt> po_tile = {a_pos.x + 2, a_pos.y + 3};
        pos_pos.push_back(po_tile);

        return {a_pos.x + 6, a_pos.y + 3};
    }
};
}  // namespace detail

template <typename Lyt>
Lyt rc_adder_gen(const uint64_t num_bits)
{

    detail::rc_adder_gen_impl<Lyt> p{num_bits};

    auto result = p.run();

    return result;
}
}  // namespace fiction

#endif  // FICTION_RC_ADDER_GEN_HPP
