// Correct: async FIFO / handshake-style CDC for arbitrary multi-bit data.
// Multi-bit data must not be re-synchronized per bit (bad/) and cannot be
// expressed as a gray counter. The FIFO uses gray-coded pointers with
// 2-FF synchronizers and a full/empty handshake. Shown here as a
// synchronizable-sketch: the two clock domains exchange gray pointers, the
// data is written and read through independent clocked ports.
module cdc_async_fifo_gray (
    input  wire        clk_wr,
    input  wire        clk_rd,
    input  wire        rst_n,
    input  wire        wr_en,
    input  wire [7:0]  wr_data,
    output reg  [7:0]  rd_data,
    output wire        full,
    output wire        empty
);
    parameter DEPTH = 8;
    parameter AW    = 3; // 2^3 = 8 entries

    reg [AW:0]   wr_ptr_bin, rd_ptr_bin;
    reg [AW:0]   wr_ptr_gray, rd_ptr_gray;
    reg [AW:0]   wr_ptr_sync1, wr_ptr_sync2; // rd domain reads wr ptr
    reg [AW:0]   rd_ptr_sync1, rd_ptr_sync2; // wr domain reads rd ptr
    reg [7:0]    mem[0:DEPTH-1];

    // Gray conversion (XOR folding).
    function [AW:0] to_gray;
        input [AW:0] b;
        to_gray = (b >> 1) ^ b;
    endfunction

    // Write pointer (wr domain).
    always @(posedge clk_wr or negedge rst_n) begin
        if (!rst_n) wr_ptr_bin <= 0;
        else if (wr_en && !full) wr_ptr_bin <= wr_ptr_bin + 1;
    end

    // Read pointer (rd domain).
    always @(posedge clk_rd or negedge rst_n) begin
        if (!rst_n) rd_ptr_bin <= 0;
        else if (!empty) begin
            rd_data <= mem[rd_ptr_bin[AW-1:0]];
            rd_ptr_bin <= rd_ptr_bin + 1;
        end
    end

    always @(posedge clk_wr) begin
        if (wr_en && !full) mem[wr_ptr_bin[AW-1:0]] <= wr_data;
    end

    // Gray pointers cross the domain boundary through 2-FF synchronizers.
    always @(posedge clk_rd or negedge rst_n) begin
        if (!rst_n) begin
            wr_ptr_sync1 <= 0;
            wr_ptr_sync2 <= 0;
        end else begin
            wr_ptr_sync1 <= wr_ptr_gray;
            wr_ptr_sync2 <= wr_ptr_sync1;
        end
    end

    always @(posedge clk_wr or negedge rst_n) begin
        if (!rst_n) begin
            rd_ptr_sync1 <= 0;
            rd_ptr_sync2 <= 0;
        end else begin
            rd_ptr_sync1 <= rd_ptr_gray;
            rd_ptr_sync2 <= rd_ptr_sync1;
        end
    end

    assign wr_ptr_gray = to_gray(wr_ptr_bin);
    assign rd_ptr_gray = to_gray(rd_ptr_bin);

    // Full/empty by comparing MSBs and the rest of the gray pointers.
    assign full  = (wr_ptr_gray == {~rd_ptr_sync2[AW], rd_ptr_sync2[AW-1:0]});
    assign empty = (rd_ptr_gray == wr_ptr_sync2);
endmodule
