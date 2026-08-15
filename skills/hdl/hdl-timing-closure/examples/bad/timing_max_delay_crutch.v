// intentionally incorrect: max_delay used as a blanket crutch across a
// path that does not meet setup. set_max_delay on a functional path
// documents the violation instead of fixing it; the hardware still samples
// invalid data at the clock edge.
module timing_max_delay_crutch (
    input  wire        clk,
    input  wire [7:0]  x,
    input  wire [7:0]  y,
    output reg  [7:0]  q
);
    // Long combinational path between x,y and q. "Fixed" with:
    //   set_max_delay 3.0 -from [all_inputs] -to [get_ports q]
    wire [15:0] prod = x * y;

    always @(posedge clk) begin
        q <= prod[7:0];
    end
endmodule
