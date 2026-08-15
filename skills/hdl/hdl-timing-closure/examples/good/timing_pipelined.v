// Correct: timing closure by real design changes — pipeline the critical
// multiply chain. Two register stages turn one 32-bit multiply + add per
// cycle into a pipelined structure that meets the target clock. The timing
// report must be green BECAUSE the logic is shorter, not because a
// constraint hid the path.
module timing_pipelined (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [15:0] a,
    input  wire [15:0] b,
    input  wire        run,
    output reg  [31:0] acc,
    output reg         valid
);
    reg [31:0] acc_q1;
    reg [31:0] acc_q2;
    reg        run_q1;
    reg        run_q2;
    reg [31:0] prod_q1;

    // Stage 0->1: compute product, register it.
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            prod_q1 <= 32'b0;
            run_q1  <= 1'b0;
        end else begin
            prod_q1 <= a * b;
            run_q1  <= run;
        end
    end

    // Stage 1->2: accumulate, register.
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            acc_q1 <= 32'b0;
            run_q2 <= 1'b0;
        end else begin
            acc_q1 <= acc_q1 + prod_q1;
            run_q2 <= run_q1;
        end
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            acc   <= 32'b0;
            valid <= 1'b0;
        end else begin
            acc   <= acc_q1;
            valid <= run_q2;
        end
    end
endmodule
