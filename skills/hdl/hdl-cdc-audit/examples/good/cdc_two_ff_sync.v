// Correct: 2-FF synchronizer for a single-bit level signal crossing clock
// domains. Two registers, nothing else between them. The first stage
// absorbs the metastable sample; the second stage provides a clean output
// for destination-domain logic. Applies to single-bit levels only.
module cdc_two_ff_sync (
    input  wire clk_dst,
    input  wire rst_n,
    input  wire async_in,
    output wire sync_out
);
    reg sync_ff1;
    reg sync_ff2;

    always @(posedge clk_dst or negedge rst_n) begin
        if (!rst_n) begin
            sync_ff1 <= 1'b0;
            sync_ff2 <= 1'b0;
        end else begin
            sync_ff1 <= async_in;   // first stage: absorb metastability
            sync_ff2 <= sync_ff1;   // second stage: clean output
        end
    end

    assign sync_out = sync_ff2;
endmodule
