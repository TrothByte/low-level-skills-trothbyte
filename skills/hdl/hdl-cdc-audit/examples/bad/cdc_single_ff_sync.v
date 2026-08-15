// intentionally incorrect: single-flop synchronizer for a CDC input.
// A single flip-flop does not provide a metastability-resolving stage for
// destination-domain logic. Classify first: single-bit level crosses need
// a 2-FF synchronizer (see good/cdc_two_ff_sync.v); this module is the
// classic "just add one flop" bug.
module cdc_single_ff_sync (
    input  wire clk_dst,
    input  wire rst_n,
    input  wire sig_src,   // async, from another clock domain
    output reg  sig_dst
);
    always @(posedge clk_dst or negedge rst_n) begin
        if (!rst_n) begin
            sig_dst <= 1'b0;
        end else begin
            sig_dst <= sig_src;  // BAD: only one stage
        end
    end
endmodule
