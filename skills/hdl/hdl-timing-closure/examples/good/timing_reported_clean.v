// Correct: the timing report itself is the artifact. This module is used
// with the documented yosys+nextpnr flow; the gate is the REPORTED WNS/
// TNS at the target frequency, reviewed for negative slack on functional
// paths (no exceptions hiding them).
module timing_reported_clean (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [7:0]  a,
    input  wire [7:0]  b,
    output reg  [7:0]  sum
);
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            sum <= 8'b0;
        end else begin
            sum <= a + b; // short path: comfortably meets 100 MHz
        end
    end
endmodule
