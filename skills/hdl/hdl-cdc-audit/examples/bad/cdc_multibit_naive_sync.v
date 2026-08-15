// intentionally incorrect: 2-FF synchronizer applied per-bit to a multi-bit
// data bus. Each bit is individually safe against metastability, but the
// bus as a whole is NOT coherent: bits arrive at the destination across
// different clock edges, so a mixed word can be captured. This is the
// highest-frequency CDC review miss. The correct approach for multi-bit
// data is gray-code (counters) or handshake/async FIFO.
module cdc_multibit_naive_sync (
    input  wire        clk_dst,
    input  wire        rst_n,
    input  wire [3:0]  data_src,   // async, multi-bit
    output reg  [3:0]  data_dst
);
    reg [3:0] sync_stage1;
    reg [3:0] sync_stage2;

    always @(posedge clk_dst or negedge rst_n) begin
        if (!rst_n) begin
            sync_stage1 <= 4'b0;
            sync_stage2 <= 4'b0;
        end else begin
            sync_stage1 <= data_src;   // BAD: each bit sampled at a
            sync_stage2 <= sync_stage1; // different edge due to skew
        end
    end

    assign data_dst = sync_stage2; // can be a mix of old and new bits
endmodule
