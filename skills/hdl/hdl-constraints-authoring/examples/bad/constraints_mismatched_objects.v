// intentionally incorrect: RTL/SDC object mismatch. The constraint references
// signals that do not exist in this module, so check_timing reports an
// unconstrained path that the author assumed was covered.
//   set_input_delay 2.0 -clock clk [get_ports {data_in enable_n}]
// There is no port named enable_n; data_in is covered, enable_n is not —
// the enable path is silently unconstrained.
module constraints_mismatched_objects (
    input  wire       clk,
    input  wire       rst_n,
    input  wire [7:0] data_in,
    input  wire       enable_n,     // enable_n exists but SDC missed the _n
    output reg  [7:0] q
);
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            q <= 8'b0;
        end else if (!enable_n) begin
            q <= data_in;
        end
    end
endmodule
