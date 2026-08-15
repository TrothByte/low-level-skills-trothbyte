// intentionally incorrect: a timing constraint is used as the CDC fix.
// set_false_path removes the path from timing analysis but does NOT
// synchronize the signal: the destination flop still samples a
// potentially-metastable value and the data can still be incoherent.
// "Constraint doesn't fix a CDC bug" — classify before fix.
module cdc_false_path_is_not_a_sync (
    input  wire        clk_src,
    input  wire        clk_dst,
    input  wire [7:0]  data_src,
    input  wire        valid_src,
    output reg  [7:0]  data_dst
);
    // An SDC false_path between clk_src and clk_dst would "clean" timing,
    // but there is no synchronizer on valid_src or the data bus.
    always @(posedge clk_dst) begin
        if (valid_src) begin   // BAD: valid_src crosses domains unsynchronized
            data_dst <= data_src;
        end
    end
    // Note: no two-flop synchronizer, no handshake, no FIFO. A false_path
    // constraint hides the violation; it does not fix the crossing.
endmodule
