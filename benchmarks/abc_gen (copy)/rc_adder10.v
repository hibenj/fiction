// Benchmark "ADD10" written by ABC on Tue Apr  9 09:10:32 2024

module ADD10 ( 
    a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, b0, b1, b2, b3, b4, b5, b6, b7,
    b8, b9,
    s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10  );
  input  a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, b0, b1, b2, b3, b4, b5,
    b6, b7, b8, b9;
  output s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
  wire \new_ADD10|c_ , new_n74_, new_n76_, new_n78_, new_n80_, new_n82_,
    \new_ADD10|0_ , new_n86_, new_n88_, new_n90_, new_n92_, new_n94_,
    \new_ADD10|1_ , new_n98_, new_n100_, new_n102_, new_n104_, new_n106_,
    \new_ADD10|2_ , new_n110_, new_n112_, new_n114_, new_n116_, new_n118_,
    \new_ADD10|3_ , new_n122_, new_n124_, new_n126_, new_n128_, new_n130_,
    \new_ADD10|4_ , new_n134_, new_n136_, new_n138_, new_n140_, new_n142_,
    \new_ADD10|5_ , new_n146_, new_n148_, new_n150_, new_n152_, new_n154_,
    \new_ADD10|6_ , new_n158_, new_n160_, new_n162_, new_n164_, new_n166_,
    \new_ADD10|7_ , new_n170_, new_n172_, new_n174_, new_n176_, new_n178_,
    \new_ADD10|8_ , new_n182_, new_n184_, new_n186_, new_n188_, new_n190_;
  assign \new_ADD10|c_  = 1'b0;
  assign new_n74_ = a0 & b0;
  assign new_n76_ = ~a0 & ~b0;
  assign new_n78_ = ~new_n74_ & ~new_n76_;
  assign new_n80_ = \new_ADD10|c_  & new_n78_;
  assign new_n82_ = ~\new_ADD10|c_  & ~new_n78_;
  assign s0 = ~new_n80_ & ~new_n82_;
  assign \new_ADD10|0_  = new_n74_ | new_n80_;
  assign new_n86_ = a1 & b1;
  assign new_n88_ = ~a1 & ~b1;
  assign new_n90_ = ~new_n86_ & ~new_n88_;
  assign new_n92_ = \new_ADD10|0_  & new_n90_;
  assign new_n94_ = ~\new_ADD10|0_  & ~new_n90_;
  assign s1 = ~new_n92_ & ~new_n94_;
  assign \new_ADD10|1_  = new_n86_ | new_n92_;
  assign new_n98_ = a2 & b2;
  assign new_n100_ = ~a2 & ~b2;
  assign new_n102_ = ~new_n98_ & ~new_n100_;
  assign new_n104_ = \new_ADD10|1_  & new_n102_;
  assign new_n106_ = ~\new_ADD10|1_  & ~new_n102_;
  assign s2 = ~new_n104_ & ~new_n106_;
  assign \new_ADD10|2_  = new_n98_ | new_n104_;
  assign new_n110_ = a3 & b3;
  assign new_n112_ = ~a3 & ~b3;
  assign new_n114_ = ~new_n110_ & ~new_n112_;
  assign new_n116_ = \new_ADD10|2_  & new_n114_;
  assign new_n118_ = ~\new_ADD10|2_  & ~new_n114_;
  assign s3 = ~new_n116_ & ~new_n118_;
  assign \new_ADD10|3_  = new_n110_ | new_n116_;
  assign new_n122_ = a4 & b4;
  assign new_n124_ = ~a4 & ~b4;
  assign new_n126_ = ~new_n122_ & ~new_n124_;
  assign new_n128_ = \new_ADD10|3_  & new_n126_;
  assign new_n130_ = ~\new_ADD10|3_  & ~new_n126_;
  assign s4 = ~new_n128_ & ~new_n130_;
  assign \new_ADD10|4_  = new_n122_ | new_n128_;
  assign new_n134_ = a5 & b5;
  assign new_n136_ = ~a5 & ~b5;
  assign new_n138_ = ~new_n134_ & ~new_n136_;
  assign new_n140_ = \new_ADD10|4_  & new_n138_;
  assign new_n142_ = ~\new_ADD10|4_  & ~new_n138_;
  assign s5 = ~new_n140_ & ~new_n142_;
  assign \new_ADD10|5_  = new_n134_ | new_n140_;
  assign new_n146_ = a6 & b6;
  assign new_n148_ = ~a6 & ~b6;
  assign new_n150_ = ~new_n146_ & ~new_n148_;
  assign new_n152_ = \new_ADD10|5_  & new_n150_;
  assign new_n154_ = ~\new_ADD10|5_  & ~new_n150_;
  assign s6 = ~new_n152_ & ~new_n154_;
  assign \new_ADD10|6_  = new_n146_ | new_n152_;
  assign new_n158_ = a7 & b7;
  assign new_n160_ = ~a7 & ~b7;
  assign new_n162_ = ~new_n158_ & ~new_n160_;
  assign new_n164_ = \new_ADD10|6_  & new_n162_;
  assign new_n166_ = ~\new_ADD10|6_  & ~new_n162_;
  assign s7 = ~new_n164_ & ~new_n166_;
  assign \new_ADD10|7_  = new_n158_ | new_n164_;
  assign new_n170_ = a8 & b8;
  assign new_n172_ = ~a8 & ~b8;
  assign new_n174_ = ~new_n170_ & ~new_n172_;
  assign new_n176_ = \new_ADD10|7_  & new_n174_;
  assign new_n178_ = ~\new_ADD10|7_  & ~new_n174_;
  assign s8 = ~new_n176_ & ~new_n178_;
  assign \new_ADD10|8_  = new_n170_ | new_n176_;
  assign new_n182_ = a9 & b9;
  assign new_n184_ = ~a9 & ~b9;
  assign new_n186_ = ~new_n182_ & ~new_n184_;
  assign new_n188_ = \new_ADD10|8_  & new_n186_;
  assign new_n190_ = ~\new_ADD10|8_  & ~new_n186_;
  assign s9 = ~new_n188_ & ~new_n190_;
  assign s10 = new_n182_ | new_n188_;
endmodule


