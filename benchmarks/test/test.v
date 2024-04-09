module top (
	a, b, o );
	input a, b;
	output o;
	wire xz|1;
	assign xz|1 = a & b;
	assign o = xz|1 ^ b;
endmodule
