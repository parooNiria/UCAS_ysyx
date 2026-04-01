module decoder_7_128(
    input  wire [ 6:0] in,
    output wire [127:0] out
);

genvar i;
generate for (i=0; i<128; i=i+1) begin : gen_for_dec_7_128
    assign out[i] = (in == i);
end endgenerate

endmodule