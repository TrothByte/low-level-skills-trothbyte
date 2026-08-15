// intentionally incorrect: timing closed by hiding the violation.
// A real timing path (a->d, critical) is declared false in SDC so the
// report goes green, but the data path still does not meet setup in
// silicon. "Never make the number green by hiding a violation." A false
// path is only legal when the path is functionally irrelevant (e.g. a
// CDC crossing that is already synchronized — see hdl-cdc-audit).
module timing_false_path_abuse (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [15:0] a,
    input  wire [15:0] b,
    input  wire        sel,
    output reg  [15:0] d
);
    // Path: sel -> mux select into d. It IS functionally meaningful and
    // timing-relevant, yet the SDC below hides it:
    //   set_false_path -from [get_ports sel] -to [get_ports d]
    wire [15:0] muxed = sel ? b : a;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            d <= 16'b0;
        end else begin
            d <= muxed;
        end
    end
endmodule
