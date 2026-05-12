`define N 16
module input_buffer(
    input logic clk,
    input logic rst_n,
    input logic [7:0] data_in,
    input logic rd_en,
    input logic wr_en,
    output logic [71:0] data_out ,
    output logic ready,
    output logic done
);
    logic [7:0] data_in_bank0 [`N*`N+9];
    logic [7:0] data_in_bank1 [`N*`N+9];
    logic [7:0] data_in_bank2 [`N*`N+9];
    logic [7:0] data_in_bank3 [`N*`N+9];
    logic [7:0] data_in_bank4 [`N*`N+9];
    logic [7:0] data_in_bank5 [`N*`N+9];
    logic [7:0] data_in_bank6 [`N*`N+9];
    logic [7:0] data_in_bank7 [`N*`N+9];
    logic [7:0] data_in_bank8 [`N*`N+9];
    logic [$clog2(`N*`N)-1:0] index_wr;
    logic [$clog2(`N*`N +9)-1:0] index_rd;
    logic [$clog2(`N*`N+9)-1:0] index_bank0;
    logic [$clog2(`N)-1 :0] counter;
    logic [$clog2(`N*`N+9)-1:0] index_bank1;
    logic [$clog2(`N*`N+9)-1:0] index_bank2;
    logic [$clog2(`N*`N+9)-1:0] index_bank3;
    logic [$clog2(`N*`N+9)-1:0] index_bank4;
    logic [$clog2(`N*`N+9)-1:0] index_bank5;
    logic [$clog2(`N*`N+9)-1:0] index_bank6;
    logic [$clog2(`N*`N+9)-1:0] index_bank7;
    logic [$clog2(`N*`N+9)-1:0] index_bank8;
    
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_wr <= 0;
            counter <=0;
            ready <= 0;
        end else begin
            if (wr_en) begin
                if (index_wr == `N -1 ) begin
                    index_wr <= 0;
                    counter <= counter + 1;
                    
                end else begin
                    index_wr <= index_wr + 1;
                    if (counter == `N -1 && index_wr == `N-2) begin
                        ready <= 1;
                    end
                    else begin
                        ready <= 0;
                    end
                    assert (counter < `N ) 
                        else $fatal("Assertion failed: counter (%0d) is less than N (%0d)", counter+1, `N);
                end
            end
            else if (done) begin
                index_wr <= 0;
                counter <=0;
                ready <= 0;
            end      
        end
    end
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_rd <= 0;
            done <= 0;
        end else begin
            if (rd_en) begin
                index_rd <= index_rd + 1;
                if (index_rd == (`N-2)*(`N-2)+7) begin
                    done <= 1;
                end
            end   
            else if (!ready) begin
                index_rd <= 0;
                done <= 0;
            
            end  
        end
    end
    //bank 0
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_bank0 <= 0;
            data_in_bank0[(`N-2)*(`N-2) + 1] <= 0;
            data_in_bank0[(`N-2)*(`N-2) + 2] <= 0;
            data_in_bank0[(`N-2)*(`N-2) + 3] <= 0;
            data_in_bank0[(`N-2)*(`N-2) + 4] <= 0;
            data_in_bank0[(`N-2)*(`N-2) + 5] <= 0;
            data_in_bank0[(`N-2)*(`N-2) + 6] <= 0;
            data_in_bank0[(`N-2)*(`N-2) + 7] <= 0;
            data_in_bank0[(`N-2)*(`N-2)] <= 0;
        end
        else begin
            if (wr_en) begin
                if (index_wr >= 0 && index_wr < `N-2 && counter >= 0 && counter < `N-2) begin
                    data_in_bank0[index_bank0] <= data_in;
                    index_bank0 <= index_bank0 + 1;
                end
            end
        end
    end
    //bank 1
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_bank1 <= 1;
            data_in_bank1[0] <= 0;
            data_in_bank1[(`N-2)*(`N-2) + 2] <= 0;
            data_in_bank1[(`N-2)*(`N-2) + 3] <= 0;
            data_in_bank1[(`N-2)*(`N-2) + 4] <= 0;
            data_in_bank1[(`N-2)*(`N-2) + 5] <= 0;
            data_in_bank1[(`N-2)*(`N-2) + 6] <= 0;
            data_in_bank1[(`N-2)*(`N-2) + 7] <= 0;
            data_in_bank1[(`N-2)*(`N-2) + 1] <= 0;
        end
        else begin
            if (wr_en) begin
                if (index_wr >= 1 && index_wr < `N-1 && counter >= 0 && counter < `N-2) begin
                    data_in_bank1[index_bank1] <= data_in;
                    index_bank1 <= index_bank1 + 1;
                end
            end
        end
    end
    //bank 2
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_bank2 <= 2;
            data_in_bank2[0] <= 0;
            data_in_bank2[1] <= 0;
            data_in_bank2[(`N-2)*(`N-2) + 3] <= 0;
            data_in_bank2[(`N-2)*(`N-2) + 4] <= 0;
            data_in_bank2[(`N-2)*(`N-2) + 5] <= 0;
            data_in_bank2[(`N-2)*(`N-2) + 6] <= 0;
            data_in_bank2[(`N-2)*(`N-2) + 7] <= 0;
            data_in_bank2[(`N-2)*(`N-2) + 2] <= 0;
        end
        else begin
            if (wr_en) begin
                if (index_wr >= 2 && index_wr < `N && counter >= 0 && counter < `N-2) begin
                    data_in_bank2[index_bank2] <= data_in;
                    index_bank2 <= index_bank2 + 1;
                end
            end
        end
    end
    //bank 3
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_bank3 <= 3;
            data_in_bank3[0] <= 0;
            data_in_bank3[1] <= 0;
            data_in_bank3[2] <= 0;
            data_in_bank3[(`N-2)*(`N-2) + 4] <= 0;
            data_in_bank3[(`N-2)*(`N-2) + 5] <= 0;
            data_in_bank3[(`N-2)*(`N-2) + 6] <= 0;
            data_in_bank3[(`N-2)*(`N-2) + 7] <= 0;
            data_in_bank3[(`N-2)*(`N-2) + 3] <= 0;
        end
        else begin
            if (wr_en) begin
                if (index_wr >= 0 && index_wr < `N-2 && counter >= 1 && counter < `N-1) begin
                    data_in_bank3[index_bank3] <= data_in;
                    index_bank3 <= index_bank3 + 1;
                end
            end
        end
    end
    //bank 4
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_bank4 <= 4;
            data_in_bank4[0] <= 0;
            data_in_bank4[1] <= 0;
            data_in_bank4[2] <= 0;
            data_in_bank4[3] <= 0;
            data_in_bank4[(`N-2)*(`N-2) + 5] <= 0;
            data_in_bank4[(`N-2)*(`N-2) + 6] <= 0;
            data_in_bank4[(`N-2)*(`N-2) + 7] <= 0;
            data_in_bank4[(`N-2)*(`N-2) + 4] <= 0;
        end
        else begin
            if (wr_en) begin
                if (index_wr >= 1 && index_wr < `N-1 && counter >= 1 && counter < `N-1) begin
                    data_in_bank4[index_bank4] <= data_in;
                    index_bank4 <= index_bank4 + 1;
                end
            end
        end
    end
    //bank 5
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_bank5 <= 5;
            data_in_bank5[0] <= 0;
            data_in_bank5[1] <= 0;
            data_in_bank5[2] <= 0;
            data_in_bank5[3] <= 0;
            data_in_bank5[4] <= 0;
            data_in_bank5[(`N-2)*(`N-2) + 6] <= 0;
            data_in_bank5[(`N-2)*(`N-2) + 7] <= 0;
            data_in_bank5[(`N-2)*(`N-2) + 5] <= 0;
        end
        else begin
            if (wr_en) begin
                if (index_wr >= 2 && index_wr < `N && counter >= 1 && counter < `N-1) begin
                    data_in_bank5[index_bank5] <= data_in;
                    index_bank5 <= index_bank5 + 1;
                end
            end
        end
    end
    //bank 6
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_bank6 <= 6;
            data_in_bank6[0] <= 0;
            data_in_bank6[1] <= 0;
            data_in_bank6[2] <= 0;
            data_in_bank6[3] <= 0;
            data_in_bank6[4] <= 0;
            data_in_bank6[5] <= 0;
            data_in_bank6[(`N-2)*(`N-2) + 7] <= 0;
            data_in_bank6[(`N-2)*(`N-2) + 6] <= 0;
        end
        else begin
            if (wr_en) begin
                if (index_wr >= 0 && index_wr < `N-2 && counter >= 2 && counter < `N) begin
                    data_in_bank6[index_bank6] <= data_in;
                    index_bank6 <= index_bank6 + 1;
                end
            end
        end
    end
    //bank 7
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_bank7 <= 7;
            data_in_bank7[0] <= 0;
            data_in_bank7[1] <= 0;
            data_in_bank7[2] <= 0;
            data_in_bank7[3] <= 0;
            data_in_bank7[4] <= 0;
            data_in_bank7[5] <= 0;
            data_in_bank7[6] <= 0;
            data_in_bank7[(`N-2)*(`N-2) + 7] <= 0;
        end
        else begin
            if (wr_en) begin
                if (index_wr >= 1 && index_wr < `N-1 && counter >= 2 && counter < `N) begin
                    data_in_bank7[index_bank7] <= data_in;
                    index_bank7 <= index_bank7 + 1;
                end
            end
        end
    end
    //bank 8
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            index_bank8 <= 8;
            data_in_bank8[0] <= 0;
            data_in_bank8[1] <= 0;
            data_in_bank8[2] <= 0;
            data_in_bank8[3] <= 0;
            data_in_bank8[4] <= 0;
            data_in_bank8[5] <= 0;
            data_in_bank8[6] <= 0;
            data_in_bank8[7] <= 0;
        end
        else begin
            if (wr_en) begin
                if (index_wr >= 2 && index_wr < `N && counter >= 2 && counter < `N) begin
                    data_in_bank8[index_bank8] <= data_in;
                    index_bank8 <= index_bank8 + 1;
                end
            end
        end
    end
    always_comb begin
        data_out = done ? '0 : {data_in_bank8[index_rd],
                                data_in_bank7[index_rd],
                                data_in_bank6[index_rd],
                                data_in_bank5[index_rd],
                                data_in_bank4[index_rd],
                                data_in_bank3[index_rd],
                                data_in_bank2[index_rd],
                                data_in_bank1[index_rd],
                                data_in_bank0[index_rd]};
    end
endmodule