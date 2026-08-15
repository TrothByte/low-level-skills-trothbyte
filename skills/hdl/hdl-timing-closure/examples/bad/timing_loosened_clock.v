// intentionally incorrect: timing "fixed" by loosening the clock until the
// report is clean. Changing the create_clock period to make WNS positive
// does not change the silicon speed grade. The report is green; the part
// still fails at the real operating frequency.
module timing_loosened_clock (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [31:0] a,
    input  wire [31:0] b,
    output reg  [31:0] acc
);
    // A long chain: acc = acc + a*b on every cycle. The real target clock
    // is 100 MHz (10 ns). SDC "fixed" by declaring 20 ns instead:
    //   create_clock -name clk -period 20.0 [get_ports clk]
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            acc <= 32'b0;
        end else begin
            acc <= acc + a * b;
        end
    end
endmodule
