// Something about this module destablizes the LW bridge
// Needs debugging

module f2h_sdram_test_master (
    input  logic        clk,
    input  logic        reset_n,

    input  logic        start,
    input  logic [31:0] read_addr,

    output logic [28:0] avm_address,
    output logic        avm_read,
    output logic [7:0]  avm_burstcount,
    input  logic [63:0] avm_readdata,
    input  logic        avm_readdatavalid,
    input  logic        avm_waitrequest,

    output logic [7:0]  result,
    output logic        done
);

    typedef enum logic [2:0] {
        STARTUP,
        IDLE,
        READ_REQ,
        READ_WAIT,
        COMPLETE
    } state_t;

    state_t      state;
    logic        start_prev;
    logic [3:0]  startup_count;

    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            state          <= STARTUP;
            startup_count  <= 4'd0;
            avm_address    <= 29'b0;
            avm_burstcount <= 8'd1;
            result         <= 8'h00;
            done           <= 1'b0;
            start_prev     <= 1'b0;
            avm_read       <= 1'b0;
        end else begin
            start_prev <= start;
            avm_read <= 1'b0;
            
            case (state)
                STARTUP: begin
                    // Hold off for 16 cycles after reset before
                    // allowing any F2SDRAM transactions
                    if (startup_count == 4'd15)
                        state <= IDLE;
                    else
                        startup_count <= startup_count + 1'b1;
                end

                IDLE: begin
                    done <= 1'b0;
                    if (start && !start_prev) begin
                        avm_address    <= read_addr[28:0];
                        avm_burstcount <= 8'd1;
                        state          <= READ_REQ;
                    end
                end

                READ_REQ: begin
                    avm_read <= 1'b1;  // ALWAYS assert while in this state

                    if (!avm_waitrequest) begin
                        state <= READ_WAIT;
                    end
                end

                READ_WAIT: begin
                    avm_read <= 1'b0;

                    if (avm_readdatavalid) begin
                        result <= avm_readdata[7:0];
                        done   <= 1'b1;
                        state  <= COMPLETE;
                    end
                end

                COMPLETE: begin
                    if (!start)
                        state <= IDLE;
                end

                default: begin
                    state <= IDLE;
                    avm_read <= 1'b0;
                end
            endcase
        end
    end

endmodule