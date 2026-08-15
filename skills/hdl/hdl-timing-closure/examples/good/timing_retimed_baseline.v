// Correct: registers physically retimed across the design — the placement/
// routing tool moves logic across register boundaries so each stage meets
// timing. This file documents the intent (a balanced pipeline); the
// physical retiming is performed by the synthesizer/placer, and the timing
// report must be reviewed for the ACTUAL WNS/TNS, not just "is green".
module timing_retimed_baseline (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [7:0]  in,
    output reg  [7:0]  out
);
    reg [7:0] mid;

    // Two balanced stages. With --flow/pack option the tool may retime.
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            mid <= 8'b0;
        end else begin
            mid <= in ^ 8'hA5;  // stage 1
        end
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            out <= 8'b0;
        end else begin
            out <= mid + 8'h3C; // stage 2
        end
    end
endmodule
