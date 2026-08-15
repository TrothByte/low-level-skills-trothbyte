// Correct: gray-code counter for crossing a counter value between clock
// domains. Consecutive gray values differ in exactly one bit, so the
// destination sees either the old or the new value, never a mix. Use for
// pointers/counters where order preservation matters; for arbitrary
// multi-bit data use handshake or async FIFO instead.
module cdc_gray_counter (
    input  wire        clk_src,
    input  wire        clk_dst,
    input  wire        rst_n,
    input  wire        count_en,
    output wire [3:0]  gray_dst
);
    reg [3:0] bin_count;
    reg [3:0] gray_src;
    reg [3:0] gray_sync1;
    reg [3:0] gray_sync2;

    // Source side: binary counter, gray-encoded.
    always @(posedge clk_src or negedge rst_n) begin
        if (!rst_n) begin
            bin_count <= 4'b0;
        end else if (count_en) begin
            bin_count <= bin_count + 4'b1;
        end
    end

    always @(*) begin
        gray_src = (bin_count >> 3)
                 ^ (bin_count >> 2)
                 ^ (bin_count >> 1)
                 ^ bin_count;
    end

    // Destination side: two-stage synchronizer per bit.
    always @(posedge clk_dst or negedge rst_n) begin
        if (!rst_n) begin
            gray_sync1 <= 4'b0;
            gray_sync2 <= 4'b0;
        end else begin
            gray_sync1 <= gray_src;
            gray_sync2 <= gray_sync1;
        end
    end

    assign gray_dst = gray_sync2;
endmodule
