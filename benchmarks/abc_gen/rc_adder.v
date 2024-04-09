// Benchmark "ADD8" written by ABC on Tue Apr  9 09:08:29 2024

module ADD8 ( 
    a0, a1, a2, a3, a4, a5, a6, a7, b0, b1, b2, b3, b4, b5, b6, b7,
    s0, s1, s2, s3, s4, s5, s6, s7, s8  );
  input  a0, a1, a2, a3, a4, a5, a6, a7, b0, b1, b2, b3, b4, b5, b6, b7;
  output s0, s1, s2, s3, s4, s5, s6, s7, s8;
  wire \new_ADD8|c_ , new_n60_, new_n62_, new_n64_, new_n66_, new_n68_,
    \new_ADD8|0_ , new_n72_, new_n74_, new_n76_, new_n78_, new_n80_,
    \new_ADD8|1_ , new_n84_, new_n86_, new_n88_, new_n90_, new_n92_,
    \new_ADD8|2_ , new_n96_, new_n98_, new_n100_, new_n102_, new_n104_,
    \new_ADD8|3_ , new_n108_, new_n110_, new_n112_, new_n114_, new_n116_,
    \new_ADD8|4_ , new_n120_, new_n122_, new_n124_, new_n126_, new_n128_,
    \new_ADD8|5_ , new_n132_, new_n134_, new_n136_, new_n138_, new_n140_,
    \new_ADD8|6_ , new_n144_, new_n146_, new_n148_, new_n150_, new_n152_;
  assign \new_ADD8|c_  = 1'b0;
  assign new_n60_ = a0 & b0;
  assign new_n62_ = ~a0 & ~b0;
  assign new_n64_ = ~new_n60_ & ~new_n62_;
  assign new_n66_ = \new_ADD8|c_  & new_n64_;
  assign new_n68_ = ~\new_ADD8|c_  & ~new_n64_;
  assign s0 = ~new_n66_ & ~new_n68_;
  assign \new_ADD8|0_  = new_n60_ | new_n66_;
  assign new_n72_ = a1 & b1;
  assign new_n74_ = ~a1 & ~b1;
  assign new_n76_ = ~new_n72_ & ~new_n74_;
  assign new_n78_ = \new_ADD8|0_  & new_n76_;
  assign new_n80_ = ~\new_ADD8|0_  & ~new_n76_;
  assign s1 = ~new_n78_ & ~new_n80_;
  assign \new_ADD8|1_  = new_n72_ | new_n78_;
  assign new_n84_ = a2 & b2;
  assign new_n86_ = ~a2 & ~b2;
  assign new_n88_ = ~new_n84_ & ~new_n86_;
  assign new_n90_ = \new_ADD8|1_  & new_n88_;
  assign new_n92_ = ~\new_ADD8|1_  & ~new_n88_;
  assign s2 = ~new_n90_ & ~new_n92_;
  assign \new_ADD8|2_  = new_n84_ | new_n90_;
  assign new_n96_ = a3 & b3;
  assign new_n98_ = ~a3 & ~b3;
  assign new_n100_ = ~new_n96_ & ~new_n98_;
  assign new_n102_ = \new_ADD8|2_  & new_n100_;
  assign new_n104_ = ~\new_ADD8|2_  & ~new_n100_;
  assign s3 = ~new_n102_ & ~new_n104_;
  assign \new_ADD8|3_  = new_n96_ | new_n102_;
  assign new_n108_ = a4 & b4;
  assign new_n110_ = ~a4 & ~b4;
  assign new_n112_ = ~new_n108_ & ~new_n110_;
  assign new_n114_ = \new_ADD8|3_  & new_n112_;
  assign new_n116_ = ~\new_ADD8|3_  & ~new_n112_;
  assign s4 = ~new_n114_ & ~new_n116_;
  assign \new_ADD8|4_  = new_n108_ | new_n114_;
  assign new_n120_ = a5 & b5;
  assign new_n122_ = ~a5 & ~b5;
  assign new_n124_ = ~new_n120_ & ~new_n122_;
  assign new_n126_ = \new_ADD8|4_  & new_n124_;
  assign new_n128_ = ~\new_ADD8|4_  & ~new_n124_;
  assign s5 = ~new_n126_ & ~new_n128_;
  assign \new_ADD8|5_  = new_n120_ | new_n126_;
  assign new_n132_ = a6 & b6;
  assign new_n134_ = ~a6 & ~b6;
  assign new_n136_ = ~new_n132_ & ~new_n134_;
  assign new_n138_ = \new_ADD8|5_  & new_n136_;
  assign new_n140_ = ~\new_ADD8|5_  & ~new_n136_;
  assign s6 = ~new_n138_ & ~new_n140_;
  assign \new_ADD8|6_  = new_n132_ | new_n138_;
  assign new_n144_ = a7 & b7;
  assign new_n146_ = ~a7 & ~b7;
  assign new_n148_ = ~new_n144_ & ~new_n146_;
  assign new_n150_ = \new_ADD8|6_  & new_n148_;
  assign new_n152_ = ~\new_ADD8|6_  & ~new_n148_;
  assign s7 = ~new_n150_ & ~new_n152_;
  assign s8 = new_n144_ | new_n150_;
endmodule


