//
// Created by benjamin on 02.05.23.
//

#ifndef FICTION_ORTHOGONAL_SEQUENTIAL_NETWORK_HPP
#define FICTION_ORTHOGONAL_SEQUENTIAL_NETWORK_HPP
#include "fiction/networks/sequential_technology_network.hpp"
#include "fiction/networks/technology_network.hpp"
#include "orthogonal.hpp"
#include "orthogonal_majority_network.hpp"

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif


namespace fiction
{
namespace detail
{

template <typename Lyt, typename Ntk>
class orthogonal_sequential_network_impl
{
  public:
    orthogonal_sequential_network_impl(const Ntk& src, const orthogonal_physical_design_params& p, orthogonal_physical_design_stats& st) :
            ntk{mockturtle::fanout_view{fanout_substitution<mockturtle::names_view<mockturtle::sequential<technology_network>>>(src)}},
            ps{p},
            pst{st}
    {}

    Lyt run()
    {
        // measure run time
        mockturtle::stopwatch stop{pst.time_total};
        // compute a coloring
        const auto ctn = east_south_edge_coloring(ntk);

        mockturtle::node_map<mockturtle::signal<Lyt>, decltype(ctn.color_ntk)> node2pos{ctn.color_ntk};

        // instantiate the layout
        //Lyt layout{determine_layout_size<Lyt>(ctn), twoddwave_clocking<Lyt>(ps.number_of_clock_phases)};
        Lyt layout{{1, 1, 1}, twoddwave_clocking<Lyt>(ps.number_of_clock_phases)};

        // reserve PI nodes without positions
        auto pi2node = reserve_input_nodes(layout, ctn.color_ntk);

        // reserve PI nodes without positions
        auto ro2node = reserve_register_output_nodes(layout, ctn.color_ntk);

        // first x-pos to use for gates is 1 because PIs take up the 0th column
        tile<Lyt> latest_pos{1, 0};
        /*if(ctn.color_ntk.isFo_inv_flag()){
            ++latest_pos.x;
        }*/
        tile<Lyt> latest_pos_inputs{0, 0};

        auto num_ris = ctn.color_ntk.num_registers();

        latest_pos.x = latest_pos.x + 2*num_ris;

        latest_pos_inputs.x = latest_pos_inputs.x + 2*num_ris;

        //std::cout<<"Inverter_Flag"<<ctn.color_ntk.isFo_inv_flag()<<std::endl;

        std::cout<<"Numpis: "<<layout.num_pis()<<std::endl;

        std::cout<<"Numros: "<<layout.num_ros()<<std::endl;

        std::cout<<"Numcis: "<<layout.num_cis()<<std::endl;

        bool fo_ri_and_po_flag{false};

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{static_cast<uint32_t>(ctn.color_ntk.size()), "[i] arranging layout: |{0}|"};
#endif

        ntk.foreach_node(
            [&, this](const auto& n, [[maybe_unused]] const auto i)
            {
                // do not place constants
                if (!ctn.color_ntk.is_constant(n))
                {
                    /*if (ctn.color_ntk.is_ri(n) && ctn.color_ntk.is_po(n))
                    {
                        std::cout << "RI und PO theoretisch" << n << std::endl;
                        ++latest_pos.y;
                        ++latest_pos.x;
                    }*/

                    bool plc_ro = false;
                    // if node is a RO, move it to its correct position
                    if constexpr (mockturtle::has_is_ro_v<Ntk>){
                        if(ctn.color_ntk.is_ro(n)){
                            plc_ro = true;
                        }else
                            plc_ro = false;
                    }

                    if(plc_ro){
                        //std::cout<<"Ro will plaziert werden"<<std::endl;
                        node2pos[n] = layout.move_node(ro2node[n], {latest_pos_inputs});
                        //std::cout<<n<<"Ro (als Pi) plaziert auf "<<"X:"<<latest_pos_inputs.x<<"Y:"<<latest_pos_inputs.y<<std::endl;

                        ctn.color_ntk.foreach_fanout(
                            n,
                            [&ctn, &n, &layout, &node2pos, &latest_pos, &latest_pos_inputs](const auto& fon)
                            {
                                if (ctn.color_ntk.color(fon) == ctn.color_south)
                                {
                                    node2pos[n] =
                                        layout.create_buf(wire_east(layout, {latest_pos_inputs}, latest_pos), latest_pos);
                                    ++latest_pos.x;
                                }

                                // PIs have only one fanout
                                return false;
                            });

                        //++latest_pos.y;
                        ++latest_pos_inputs.y;
                        latest_pos.y=latest_pos_inputs.y;
                    }

                    // if node is a PI, move it to its correct position
                    else if (ctn.color_ntk.is_pi(n))
                    {
                        //node2pos[n] = layout.move_node(pi2node[n], {latest_pos_inputs});
                        node2pos[n] = layout.move_node(pi2node[n], {0, latest_pos_inputs.y});
                        //std::cout<<n<<"Pi plaziert auf "<<"X:"<<latest_pos_inputs.x<<"Y:"<<latest_pos_inputs.y<<std::endl;

                        // resolve conflicting PIs
                        ctn.color_ntk.foreach_fanout(
                            n,
                            [&ctn, &n, &layout, &node2pos, &latest_pos, &latest_pos_inputs](const auto& fon)
                            {
                                if (ctn.color_ntk.color(fon) == ctn.color_south)
                                {
                                    node2pos[n] =
                                        layout.create_buf(wire_east(layout, {0, latest_pos_inputs.y}, latest_pos), latest_pos);
                                    ++latest_pos.x;
                                }
                                if (ctn.color_ntk.color(fon) == ctn.color_east)
                                {
                                    node2pos[n] =
                                        layout.create_buf(wire_east(layout, {0, latest_pos_inputs.y}, latest_pos_inputs), latest_pos_inputs);
                                    ++latest_pos.x;
                                }

                                // PIs have only one fanout
                                return false;
                            });

                        ++latest_pos_inputs.y;
                        latest_pos.y=latest_pos_inputs.y;
                    }
                    // if n has only one fanin
                    else if (const auto fc = fanins(ctn.color_ntk, n); fc.fanin_nodes.size() == 1)
                    {
                        const auto& pre = fc.fanin_nodes[0];

                        const auto pre_t = static_cast<tile<Lyt>>(node2pos[pre]);

                        /*if(ctn.color_ntk.is_fanout(n) && ctn.color_ntk.is_pi(pre)){
                            ctn.color_ntk.paint(mockturtle::node<Ntk>{n}, ctn.color_east);
                            ++latest_pos.y;
                        }*/

                        // n is colored east
                        if (const auto clr = ctn.color_ntk.color(n); clr == ctn.color_east)
                        {
                            auto insert_position = latest_pos.x;
                            /*if(ntk.isFo_inv_flag() && ntk.is_inv(n) && ntk.is_pi(pre)){
                                insert_position = 1;
                                --latest_pos.x;
                                std::cout<<"INV";
                            }*/
                            /*oder ist inverter und nächster knoten ist pi*/
                            if(ctn.color_ntk.is_fanout(n) && ctn.color_ntk.is_pi(pre))
                            {
                                ++latest_pos.y;
                            }
                            const tile<Lyt> t{insert_position, pre_t.y};

                            /*if(ctn.color_ntk.is_fanout(n)){
                                std::cout<<n<<"FO plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                            }else{
                                std::cout<<n<<"Inv plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                            }

                            std::cout<<n<<"Pre"<<pre<<std::endl;
                            std::cout<<n<<"color: "<<"east"<<std::endl;*/
                            node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre_t);
                            ++latest_pos.x;
                        }
                        // n is colored south
                        else if (clr == ctn.color_south)
                        {
                            if((ctn.color_ntk.is_inv(n) ||ctn.color_ntk.is_fanout(n)) && latest_pos.y<latest_pos_inputs.y){
                                const tile<Lyt> t{pre_t.x, latest_pos_inputs.y};
                                /*if(ctn.color_ntk.is_fanout(n)){
                                    std::cout<<n<<"FO plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                                }else{
                                    std::cout<<n<<"Inv plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                                }
                                std::cout<<n<<"Pre"<<pre<<std::endl;
                                std::cout<<n<<"color: "<<"south"<<std::endl;*/
                                node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre_t);
                                latest_pos.y = t.y+1;
                            }
                            else
                            {
                                const tile<Lyt> t{pre_t.x, latest_pos.y};
                                /*if(ctn.color_ntk.is_fanout(n)){
                                    std::cout<<n<<"FO plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                                }else{
                                    std::cout<<n<<"Inv plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                                }
                                std::cout<<n<<"Pre"<<pre<<std::endl;
                                std::cout<<n<<"color: "<<"south"<<std::endl;*/
                                node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre_t);
                                ++latest_pos.y;
                            }
                        }
                        else
                        {
                            // single fanin nodes should not be colored null
                            assert(false);
                        }
                    }
                    else  // if node has two fanins (or three fanins with one of them being constant)
                    {
                        const auto &pre1 = fc.fanin_nodes[0], pre2 = fc.fanin_nodes[1];

                        auto pre1_t = static_cast<tile<Lyt>>(node2pos[pre1]),
                             pre2_t = static_cast<tile<Lyt>>(node2pos[pre2]);

                        tile<Lyt> t{};

                        auto fo_ri_po = fanouts(ctn.color_ntk, pre1);

                        /*if(fo_ri_po.size()==2)
                        {
                            if ((ctn.color_ntk.is_ri(fo_ri_po[0]) && ctn.color_ntk.is_po(fo_ri_po[0])) ||
                                (ctn.color_ntk.is_ri(fo_ri_po[1]) && ctn.color_ntk.is_po(fo_ri_po[1])))
                            {
                                std::cout << "RI und PO" << pre1 << std::endl;
                                ++latest_pos.y;
                                ++latest_pos.x;
                                fo_ri_and_po_flag = true;
                            }
                        }
                        fo_ri_po = fanouts(ctn.color_ntk, pre2);

                        if(fo_ri_po.size()==2)
                        {
                            if ((ctn.color_ntk.is_ri(fo_ri_po[0]) && ctn.color_ntk.is_po(fo_ri_po[0])) ||
                                (ctn.color_ntk.is_ri(fo_ri_po[1]) && ctn.color_ntk.is_po(fo_ri_po[1])))
                            {
                                std::cout << "RI und PO" << pre1 << std::endl;
                                ++latest_pos.y;
                                ++latest_pos.x;
                                fo_ri_and_po_flag = true;
                            }
                        }*/

                        // n is colored east
                        if (const auto clr = ctn.color_ntk.color(n); clr == ctn.color_east)
                        {
                            // make sure pre1_t is the northward tile
                            if (pre2_t.y < pre1_t.y){
                                std::swap(pre1_t, pre2_t);
                            }


                            // use larger y position of predecessors
                            t = {latest_pos.x, pre2_t.y};

                            // each 2-input gate has one incoming bent wire
                            pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {t.x + 1, pre1_t.y}));

                            ++latest_pos.x;
                            if(latest_pos.y < latest_pos_inputs.y){
                                latest_pos.y = t.y+1;
                            }

                            /*std::cout<<n<<"And plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                            std::cout<<n<<"Pre1: "<<pre1<<std::endl;
                            std::cout<<n<<"Pre2: "<<pre2<<std::endl;
                            std::cout<<n<<"color: "<<"east"<<std::endl;*/

                        }
                        // n is colored south
                        else if (clr == ctn.color_south)
                        {
                            auto pre_fo = pre2;
                            // pre2_t is the westward tile
                            if (pre2_t.x > pre1_t.x){
                                std::swap(pre1_t, pre2_t);
                                pre_fo = pre1;
                            }

                            /**NEW CODE
                             * !!new south wire option
                             * **/
                            // check if pre1_t is now also the northward tile
                            if (pre1_t.y < pre2_t.y && !ctn.color_ntk.is_fanout(pre_fo))
                            {
                                if(pre2_t.x == pre1_t.x)
                                {
                                    // node can be placed on y position of pre2_t
                                    //std::cout<<"Neue south wires für: "<<n<<std::endl;

                                    // use larger x position of predecessors
                                    t = {latest_pos.x, pre2_t.y};
                                    /*std::cout<<n<<"And plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                                    std::cout<<n<<"Pre1: "<<pre1<<std::endl;
                                    std::cout<<n<<"Pre2: "<<pre2<<std::endl;
                                    std::cout<<n<<"color: "<<"south"<<std::endl;*/
                                    ++latest_pos.x;
                                }
                                else
                                {
                                    // node can be placed on y position of pre2_t
                                    //std::cout<<"Neue south wires für: "<<n<<std::endl;

                                    // use larger x position of predecessors
                                    t = {pre1_t.x, pre2_t.y};
                                    /* std::cout<<n<<"And plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                                     std::cout<<n<<"Pre1: "<<pre1<<std::endl;
                                     std::cout<<n<<"Pre2: "<<pre2<<std::endl;
                                     std::cout<<n<<"color: "<<"south"<<std::endl;*/
                                }
                                if(pre2_t.y+1>latest_pos.y)
                                {
                                    latest_pos.y =pre2_t.y+1;
                                }
                            }
                            /**!!**************************************************************************************/
                            else
                            {
                                if(latest_pos.y<latest_pos_inputs.y)
                                {
                                    // use larger x position of predecessors
                                    t = {pre1_t.x, latest_pos_inputs.y};
                                    // each 2-input gate has one incoming bent wire
                                    pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, t.y + 1}));

                                    latest_pos.y = t.y+1;
                                    /*std::cout<<n<<"And plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                                    std::cout<<n<<"Pre1: "<<pre1<<std::endl;
                                    std::cout<<n<<"Pre2: "<<pre2<<std::endl;
                                    std::cout<<n<<"color: "<<"south"<<std::endl;*/
                                }
                                else
                                {
                                    // use larger x position of predecessors
                                    t = {pre1_t.x, latest_pos.y};

                                    // each 2-input gate has one incoming bent wire
                                    pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, t.y + 1}));

                                    ++latest_pos.y;
                                    /*std::cout<<n<<"And plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                                    std::cout<<n<<"Pre1: "<<pre1<<std::endl;
                                    std::cout<<n<<"Pre2: "<<pre2<<std::endl;
                                    std::cout<<n<<"color: "<<"south"<<std::endl;*/
                                }
                            }
                        }
                        // n is colored null; corner case
                        else
                        {
                            // make sure pre1_t has an empty tile to its east and pre2_t to its south
                            std::swap(pre1_t, pre2_t);
                            if (!layout.is_empty_tile(layout.east(pre1_t)) ||
                                !layout.is_empty_tile(layout.south(pre2_t)))
                                std::swap(pre1_t, pre2_t);

                            const auto ttt = layout.get_node(layout.east(pre1_t));

                            t = latest_pos;

                            /* std::cout<<n<<"And plaziert auf"<<"X:"<<t.x<<"Y:"<<t.y<<std::endl;
                             std::cout<<n<<"Pre1: "<<pre1<<std::endl;
                             std::cout<<n<<"Pre2: "<<pre2<<std::endl;
                             std::cout<<n<<"color: "<<"southeast"<<std::endl;*/

                            // both wires have one bent
                            pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {t.x + 1, pre1_t.y}));
                            pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, t.y + 1}));

                            ++latest_pos.x;
                            ++latest_pos.y;
                        }

                        node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre1_t, pre2_t, fc.constant_fanin);
                    }

                    //std::cout<<"latest X: "<<latest_pos.x<<std::endl;
                    //std::cout<<"latest Y: "<<latest_pos.y<<std::endl;

                    // create PO at applicable position
                    /*if (ctn.color_ntk.is_po(n))
                    {
                        if (!is_eastern_po_orientation_available(ctn, n))
                        {
                            ++latest_pos.y;
                        }
                    }*/

                    /*std::cout<<"latest X after PO: "<<latest_pos.x<<std::endl;
                    std::cout<<"latest Y after PO: "<<latest_pos.y<<std::endl;*/

                }


#if (PROGRESS_BARS)
                // update progress
                bar(i);
#endif
            });

        /**********************************************************Begin: Place Ris***************************************************************/
        // The RIs need to be placed in the same order as the ROs at the Inputs
        //
        //
        tile<Lyt> ri_tile{};
        int repos{0};
        int one_reg_rsz_flag{0};
        if (num_ris == 1)
        {
            one_reg_rsz_flag = 1;
        }

        if constexpr (mockturtle::has_is_ro_v<Ntk>)
        {
            if (!ctn.color_ntk.is_combinational())
            {
                /*if(!fo_ri_and_po_flag)//&& num_ris != 1
                {
                    ++latest_pos.x;
                }*/

                ctn.color_ntk.foreach_ri(
                    [&](const auto& n)
                    {
                        if(!(is_eastern_po_orientation_available(ctn, n) && !ctn.color_ntk.is_po(n)))
                        {
                            ++repos;
                        }
                    });

                ctn.color_ntk.foreach_ri(
                    [&](const auto& n)
                    {
                        //std::cout << "Ro nodes " << ctn.color_ntk.ro_index(ctn.color_ntk.ri_to_ro(n)) << std::endl;

                        auto reg_number = ctn.color_ntk.ro_index(ctn.color_ntk.ri_to_ro(n));

                        if (ctn.color_ntk.is_ri(n))
                        {
                            const auto ffs = fanins(ctn.color_ntk, n);

                            const auto &pre = ffs.fanin_nodes[0];

                            auto n_s = node2pos[n];

                            tile<Lyt> original_ri_position = static_cast<tile<Lyt>>(n_s);

                            tile<Lyt> ri_tile{};

                            ri_tile = static_cast<tile<Lyt>>(n_s);

                            // determine PO orientation
                            if (is_eastern_po_orientation_available(ctn, n) && !ctn.color_ntk.is_po(n))
                            {
                                ri_tile = static_cast<tile<Lyt>>(n_s);
                            }
                            else
                            {
                                --repos;
                                ri_tile = static_cast<tile<Lyt>>(n_s);
                                ri_tile = static_cast<tile<Lyt>>(wire_south(layout, ri_tile, {ri_tile.x, latest_pos.y+1}));
                                ++latest_pos.y;
                            }

                            // check if PO position is located at the border
                            /*if (layout.is_at_eastern_border({ri_tile.x + 1, ri_tile.y}))
                            {
                                //++ri_tile.x;
                                layout.create_ri(n_s,
                                                 ctn.color_ntk.has_output_name(po_counter) ?
                                                     ctn.color_ntk.get_output_name(po_counter++) :
                                                     fmt::format("po{}", po_counter++),
                                                 ri_tile);
                            }
                            // place PO at the border and connect it by wire segments
                            else
                            {*/
                            const tile<Lyt> anker{ri_tile};
                            ri_tile = {latest_pos.x, ri_tile.y};
                            layout.create_ri(wire_east(layout, anker, ri_tile),
                                             ctn.color_ntk.has_output_name(po_counter) ?
                                                 ctn.color_ntk.get_output_name(po_counter++) :
                                                 fmt::format("po{}", po_counter++),
                                             ri_tile);
                            //}
                            ++latest_pos.x;
                            /***********************************************************End: Place Ris***************************************************************/
                            /*std::cout << n << "RI plaziert auf"
                                      << "X:" << ri_tile.x << "Y:" << ri_tile.y << std::endl;*/

                            /**********************************************************Begin: Wire Registers***************************************************************/
                            /*std::cout << n << "latest pos "
                                      << "X:" << latest_pos.x << "Y:" << latest_pos.y << std::endl;*/
                            ri_tile = static_cast<tile<Lyt>>(wire_south(layout, ri_tile, {ri_tile.x, latest_pos.y + 1 + reg_number + repos}));

                            //std::cout << n << "repos "<<repos<<std::endl;

                            auto check = static_cast<tile<Lyt>>(node2pos[3]);
                            //std::cout<<check.x<<std::endl;
                            //std::cout<<check.y<<std::endl;

                            //case with only one register: smaller solution possible
                            if(num_ris == 1)
                            {
                                auto checking_n = ctn.color_ntk.ro_at(reg_number);
                                //std::cout<<"checking_n "<<checking_n<<std::endl;
                                auto wire_to_ri_tile = static_cast<tile<Lyt>>(node2pos[checking_n]);
                                //std::cout<<"checking "<<wire_to_ri_tile.x<<std::endl;
                                //std::cout<<"checking "<<wire_to_ri_tile.y<<std::endl;
                                auto checking = static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]);
                                checking.x = latest_pos_inputs.x;
                                if (    (static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y - ri_tile.y) % 2 !=
                                    (latest_pos_inputs.x - ri_tile.x) % 2 + reg_number)
                                {
                                    --checking.x;
                                    --checking.y;
                                }
                                //layout.resize({ri_tile.x, ri_tile.y, 1});

                                auto pre_clock = layout.get_clock_number({ri_tile});
                                if(checking.x < 1)
                                {
                                    for(auto index = ri_tile.x - 1; index > checking.x; --index){
                                        layout.assign_clock_number({index,ri_tile.y,0}, pre_clock+1);
                                        layout.assign_clock_number({index,ri_tile.y,1}, pre_clock+1);
                                        pre_clock = layout.get_clock_number({index, ri_tile.y});
                                    }
                                    layout.assign_clock_number({0,ri_tile.y,0}, pre_clock+1);
                                    layout.assign_clock_number({0,ri_tile.y,1}, pre_clock+1);
                                    ri_tile = static_cast<tile<Lyt>>(wire_west(layout, ri_tile, {checking.x, ri_tile.y}));
                                    ri_tile = static_cast<tile<Lyt>>(wire_west(layout, ri_tile, {checking.x, ri_tile.y}));
                                    /*std::cout<<ri_tile.x<<std::endl;*/
                                }
                                else
                                {
                                    for(auto index = ri_tile.x - 1; index > checking.x - 1; --index){
                                        layout.assign_clock_number({index,ri_tile.y,0}, pre_clock+1);
                                        layout.assign_clock_number({index,ri_tile.y,1}, pre_clock+1);
                                        pre_clock = layout.get_clock_number({index, ri_tile.y});
                                    }
                                    ri_tile = static_cast<tile<Lyt>>(wire_west(layout, ri_tile, {checking.x - 0*one_reg_rsz_flag - 1, ri_tile.y}));
                                }

                                pre_clock = layout.get_clock_number({ri_tile});
                                if(checking.y + 1 < 1)
                                {
                                    for(auto index = ri_tile.y - 1; index > checking.y; --index){
                                        layout.assign_clock_number({ri_tile.x,index,0}, pre_clock+1);
                                        pre_clock = layout.get_clock_number({ri_tile.x, index});
                                    }
                                    ri_tile = static_cast<tile<Lyt>>(wire_north(layout, ri_tile, {ri_tile.x, checking.y}));
                                    ri_tile = static_cast<tile<Lyt>>(wire_north(layout, ri_tile, {ri_tile.x, checking.y}));
                                }else
                                {
                                    for(auto index = ri_tile.y - 1; index > checking.y; --index){
                                        layout.assign_clock_number({ri_tile.x,index,0}, pre_clock+1);
                                        pre_clock = layout.get_clock_number({ri_tile.x, index});
                                    }
                                    ri_tile = static_cast<tile<Lyt>>(wire_north(layout, ri_tile, {ri_tile.x, checking.y}));
                                }

                                return;
                            }

                            //general solution for N registers
                            auto ro_position = node2pos[ctn.color_ntk.ri_to_ro(n)];
                            tile<Lyt> ri_tile_ro_pos = static_cast<tile<Lyt>>(ro_position);
                            /*std::cout<<"reg_number"<<reg_number<<std::endl;
                            std::cout<<"ro_pos"<<floor((ri_tile_ro_pos.y)/4)<<std::endl;*/
                            auto g_syc_const = floor((ri_tile_ro_pos.y)/4);
                            ri_tile = static_cast<tile<Lyt>>(wire_east(layout, ri_tile, {latest_pos.x + num_ris + reg_number*3 + g_syc_const*2, ri_tile.y}));


                            //wire south to latest_pos.y + num_ris
                            auto pre_clock = layout.get_clock_number({ri_tile});
                            tile<Lyt> checking{ri_tile.x, ri_tile.y + (num_ris-1-reg_number)*2 + 2};
                            ri_tile = static_cast<tile<Lyt>>(wire_south(layout, ri_tile, checking));
                            //layout.resize({ri_tile.x, ri_tile.y+1 +south_ri+num_ris-2, 1});
                            pre_clock = layout.get_clock_number({ri_tile});

                            if (    (static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y - ri_tile.y) % 2 !=
                                (latest_pos_inputs.x - ri_tile.x) % 2)
                            {
                                for(auto index = ri_tile.x - 1; index > latest_pos_inputs.x - 3 - 2*(num_ris-reg_number-1)+1; --index){
                                    layout.assign_clock_number({index,ri_tile.y,0}, pre_clock+1);
                                    layout.assign_clock_number({index,ri_tile.y,1}, pre_clock+1);
                                    pre_clock = layout.get_clock_number({index, ri_tile.y});
                                }
                                ri_tile = static_cast<tile<Lyt>>(wire_west(layout, ri_tile, {latest_pos_inputs.x - 3 - 2*(num_ris-reg_number-1)+1, ri_tile.y}));
                            }
                            else
                            {
                                if((latest_pos_inputs.x - 2*(num_ris-reg_number-1)) < 3)
                                {
                                    for(auto index = ri_tile.x - 1; index > latest_pos_inputs.x - 3 - 2*(num_ris-reg_number-1)+1; --index){
                                        layout.assign_clock_number({index,ri_tile.y,0}, pre_clock+1);
                                        layout.assign_clock_number({index,ri_tile.y,1}, pre_clock+1);
                                        pre_clock = layout.get_clock_number({index, ri_tile.y});
                                    }
                                    layout.assign_clock_number({0,ri_tile.y,0}, pre_clock+1);
                                    layout.assign_clock_number({0,ri_tile.y,1}, pre_clock+1);
                                    ri_tile = static_cast<tile<Lyt>>(wire_west(layout, ri_tile, {latest_pos_inputs.x - 3 - 2*(num_ris-reg_number-1)+1, ri_tile.y}));
                                    ri_tile = static_cast<tile<Lyt>>(wire_west(layout, ri_tile, {latest_pos_inputs.x - 3 - 2*(num_ris-reg_number-1)+1, ri_tile.y}));
                                    /*std::cout<<ri_tile.x<<std::endl;*/
                                }
                                else
                                {
                                    for(auto index = ri_tile.x - 1; index > latest_pos_inputs.x - 3 - 2*(num_ris-reg_number-1); --index){
                                        layout.assign_clock_number({index,ri_tile.y,0}, pre_clock+1);
                                        layout.assign_clock_number({index,ri_tile.y,1}, pre_clock+1);
                                        pre_clock = layout.get_clock_number({index, ri_tile.y});
                                    }
                                    ri_tile = static_cast<tile<Lyt>>(wire_west(layout, ri_tile, {latest_pos_inputs.x - 3 - 2*(num_ris-reg_number-1), ri_tile.y}));
                                }

                            }

                            pre_clock = layout.get_clock_number({ri_tile});
                            if(static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y == 0)
                            {
                                for(auto index = ri_tile.y - 1; index > static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y; --index){
                                    layout.assign_clock_number({ri_tile.x,index,0}, pre_clock+1);
                                    pre_clock = layout.get_clock_number({ri_tile.x, index});
                                }
                                ri_tile = static_cast<tile<Lyt>>(wire_north(layout, ri_tile, {ri_tile.x, static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y}));
                                ri_tile = static_cast<tile<Lyt>>(wire_north(layout, ri_tile, {ri_tile.x, static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y}));
                            }else
                            {
                                for(auto index = ri_tile.y - 1; index > static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y - 1; --index){
                                    layout.assign_clock_number({ri_tile.x,index,0}, pre_clock+1);
                                    pre_clock = layout.get_clock_number({ri_tile.x, index});
                                }
                                auto wannaknow = static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y-1;
                                ri_tile = static_cast<tile<Lyt>>(wire_north(layout, ri_tile, {ri_tile.x, static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y - 1}));
                            }

                            ri_tile = static_cast<tile<Lyt>>(wire_east(layout, ri_tile, {latest_pos_inputs.x, ri_tile.y}));

                            // auto current_ri = static_cast<mockturtle::signal<Ntk>>(n);
                        }


                    });
            }
            auto new_field_cis = floor((layout.num_cis()-1)/4);
            layout.resize({latest_pos.x + (num_ris-1)*4 + new_field_cis*2, latest_pos.y-1 + (num_ris)*2 +repos, 1});
        }

        // Wire RIs back to ROs
        /*int x = 1;
        switch (x)
        {
            case 1:
                // R1
                // wire east to latest_pos.x + 1
                // wire south to latest_pos.y + num_ris
                // wire west to if (ri.y - ro.y log2 = (ri.x - ri.x + num_ris) log2) {pos_inputs.x - 2 - 2*(2)} else {pos_inputs.x - 3 - 2*(2)} wire north to corresponding ro.y wire east to corresponding ro.x
                // log2 (static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(ri_ix)]).y - static_cast<tile<Lyt>>(node2pos[static_cast<mockturtle::node<Ntk>>(ctn.color_ntk.ri_at(ri_ix))]).y) ==
                // log2 (static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(ri_ix)]).x - static_cast<tile<Lyt>>(node2pos[static_cast<mockturtle::node<Ntk>>(ctn.color_ntk.ri_at(ri_ix))]).x + num_ris)
                break;
            case 2:
                // R1
                // wire east to latest_pos.x + 1 + one cycle
                // wire south to latest_pos.y + num_ris - 1
                // wire west to if (ri.y - ro.y log2 = (ri.x - ri.x + num_ris) log2) {pos_inputs.x - 2 - 1*(2)} else {pos_inputs.x - 3 - 1*(2)} wire north to corresponding ro.y wire east to corresponding ro.x
                break;
            case 3:
                // R1
                // wire east to latest_pos.x + 1 + two cycles
                // wire south to latest_pos.y + num_ris - 2
                // wire west to if (ri.y - ro.y log2 = (ri.x - ri.x + num_ris) log2) {pos_inputs.x - 2 - 0*(2)} else {pos_inputs.x - 3 - 0*(2)} wire north to corresponding ro.y wire east to corresponding ro.x
                break;
        }
        ++ri_ix;*/
        /***********************************************************End: Wire Registers***************************************************************/
        /**********************************************************Begin: Place Pos***************************************************************/
        //Since the layout size is only known after placing all gates, the POs have to be placed after the main algorithm
        //
        //
        ctn.color_ntk.foreach_po(
            [&](const auto& po){
                const auto n_s = node2pos[po];

                tile<Lyt> po_tile{};

                // determine PO orientation
                if (is_eastern_po_orientation_available(ctn, po))
                {
                    po_tile = static_cast<tile<Lyt>>(n_s);
                    //++po_tile.x;
                    //++latest_pos.x;
                    if(ctn.color_ntk.is_combinational())
                    {
                        layout.resize({latest_pos.x, latest_pos.y-1, 1});
                    }
                }
                else
                {
                    po_tile = static_cast<tile<Lyt>>(n_s);
                    //++latest_pos.y;
                    po_tile = static_cast<tile<Lyt>>(wire_south(layout, po_tile, {po_tile.x, po_tile.y + 2}));
                    //++po_tile.y;
                    if(ctn.color_ntk.is_combinational())
                    {
                        layout.resize({latest_pos.x, latest_pos.y - 1, 1});
                    }
                }

                // check if PO position is located at the border
                if (layout.is_at_eastern_border({po_tile.x + 1, po_tile.y}))
                {
                    ++po_tile.x;
                    layout.create_po(n_s,
                                     ctn.color_ntk.has_output_name(po_counter) ?
                                         ctn.color_ntk.get_output_name(po_counter++) :
                                         fmt::format("po{}", po_counter++),
                                     po_tile);
                }
                // place PO at the border and connect it by wire segments
                else
                {
                    const tile<Lyt> anker{po_tile};

                    po_tile = layout.eastern_border_of(po_tile);

                    layout.create_po(wire_east(layout, anker, po_tile),
                                     ctn.color_ntk.has_output_name(po_counter) ?
                                         ctn.color_ntk.get_output_name(po_counter++) :
                                         fmt::format("po{}", po_counter++),
                                     po_tile);
                }
                //std::cout<<po<<"PO plaziert auf"<<"X:"<<po_tile.x<<"Y:"<<po_tile.y<<std::endl;
            });
        /**********************************************************End: Place Pos***************************************************************/

        // restore possibly set signal names
        restore_names(ctn.color_ntk, layout, node2pos);

        int crossing_count{0};
        layout.foreach_tile(
            [&layout, &crossing_count](const auto& t)
            {
                const auto nde = layout.get_node(t);
                if(layout.is_wire(nde))
                {
                    if(t.z == 1)
                    {
                        ++crossing_count;
                    }
                }
            });
        std::cout<<"Crossing_Num: "<<crossing_count<<std::endl;

        // restore possibly set signal names
        restore_names(ctn.color_ntk, layout, node2pos);

        // statistical information
        pst.x_size    = layout.x() + 1;
        pst.y_size    = layout.y() + 1;
        pst.num_gates = layout.num_gates();
        pst.num_wires = layout.num_wires();

        std::cout<<"ntk.num_gates()"<<ntk.num_gates()<<std::endl;

        std::cout<<"ntk.num_Reg" <<ctn.color_ntk.num_registers()<<std::endl;

        return layout;
    }

  private:
    mockturtle::topo_view<mockturtle::fanout_view<mockturtle::names_view<mockturtle::sequential<technology_network>>>> ntk;

    orthogonal_physical_design_params ps;
    orthogonal_physical_design_stats& pst;

    uint32_t po_counter{0};
};

}  // namespace detail


template <typename Lyt, typename Ntk>
Lyt orthogonal_sequential_network(const Ntk& ntk, orthogonal_physical_design_params ps = {},
                                  orthogonal_physical_design_stats* pst = nullptr)
{
    static_assert(is_gate_level_layout_v<Lyt>, "Lyt is not a gate-level layout");
    static_assert(mockturtle::is_network_type_v<Ntk>,
                  "Ntk is not a network type");  // Ntk is being converted to a topology_network anyway, therefore,
                                                 // this is the only relevant check here

    // check for input degree
    if (has_high_degree_fanin_nodes(ntk, 3))
    {
        throw high_degree_fanin_exception();
    }

    orthogonal_physical_design_stats  st{};
    detail::orthogonal_sequential_network_impl<Lyt, Ntk> p{ntk, ps, st};

    auto result = p.run();

    if (pst)
    {
        *pst = st;
    }

    return result;
}

}  // namespace fiction
#endif  // FICTION_ORTHOGONAL_SEQUENTIAL_NETWORK_HPP
