// Benchmark "1hot_8vars" written by ABC on Tue Apr  9 09:18:46 2024

module \1hot_8vars  ( 
    i0, i1, i2, i3, i4, i5, i6, i7,
    o00, o01, o02, o03, o04, o05, o06, o07, o08, o09, o10, o11, o12, o13,
    o14, o15, o16, o17, o18, o19, o20, o21, o22, o23, o24, o25, o26, o27  );
  input  i0, i1, i2, i3, i4, i5, i6, i7;
  output o00, o01, o02, o03, o04, o05, o06, o07, o08, o09, o10, o11, o12, o13,
    o14, o15, o16, o17, o18, o19, o20, o21, o22, o23, o24, o25, o26, o27;
  assign o00 = ~i0 | ~i1;
  assign o01 = ~i0 | ~i2;
  assign o02 = ~i0 | ~i3;
  assign o03 = ~i0 | ~i4;
  assign o04 = ~i0 | ~i5;
  assign o05 = ~i0 | ~i6;
  assign o06 = ~i0 | ~i7;
  assign o07 = ~i1 | ~i2;
  assign o08 = ~i1 | ~i3;
  assign o09 = ~i1 | ~i4;
  assign o10 = ~i1 | ~i5;
  assign o11 = ~i1 | ~i6;
  assign o12 = ~i1 | ~i7;
  assign o13 = ~i2 | ~i3;
  assign o14 = ~i2 | ~i4;
  assign o15 = ~i2 | ~i5;
  assign o16 = ~i2 | ~i6;
  assign o17 = ~i2 | ~i7;
  assign o18 = ~i3 | ~i4;
  assign o19 = ~i3 | ~i5;
  assign o20 = ~i3 | ~i6;
  assign o21 = ~i3 | ~i7;
  assign o22 = ~i4 | ~i5;
  assign o23 = ~i4 | ~i6;
  assign o24 = ~i4 | ~i7;
  assign o25 = ~i5 | ~i6;
  assign o26 = ~i5 | ~i7;
  assign o27 = ~i6 | ~i7;
endmodule


