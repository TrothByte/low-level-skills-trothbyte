// Correct RTL that the good SDC constrains: single clock, explicit input/
// output registers, a documented async input (synchronized elsewhere), and
// an accumulator that legitimately runs every 4 cycles. Constraint and RTL
// must agree object-for-object.
module constraints_authoring_target (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [7:0]  data_in,
    input  wire        async_in,   // synchronized elsewhere (2-FF)
    output reg  [7:0]  data_out,
    output reg  [7:0]  acc
);
    reg [1:0] cycle;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            cycle <= 2'b0;
        end else begin
            cycle <= cycle + 2'b1;
        end
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            data_out <= 8'b0;
        end else if (cycle == 2'b11) begin
            data_out <= data_in;    // registered output
        end
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            acc <= 8'b0;
        end else if (cycle == 2'b11) begin
            acc <= acc + data_in;   // 4-cycle accumulate
        end
    end
endmodule
