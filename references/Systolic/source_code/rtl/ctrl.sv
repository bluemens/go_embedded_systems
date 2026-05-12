module ctrl(
    input  logic        clk,
    input  logic        rst_n,
    input  logic        input_data_ready,    
    input  logic [71:0] input_data,          
    input  logic        weight_data_ready,   
    input  logic [7:0]  weight_data,         
    output logic        output_start,        
    output logic        input_start,         
    output logic        weight_start,        
    output logic [15:0] output_data          
);


    typedef enum logic [2:0] {
        IDLE,                  
        LOAD_WEIGHT,           
        PIPELINE_PROCESSING,   
        FINISH_PROCESSING,     
        OUTPUT_RESULT          
    } state_t;
    
    state_t current_state, next_state;
    

    logic [7:0] weight_reg;           
    logic [71:0] input_reg;           
    logic [15:0] output_reg;          
    
    logic unsigned [3:0] weight_counter;       
    logic weight_loaded;              
    logic processing_done;            
    
    logic [7:0] data_counter;         
    logic [7:0] output_counter;       
    

    localparam MAX_WEIGHTS = 9;      
    localparam MAX_DATA_COUNT = 205; 
    localparam OUTPUT_CYCLES = 10;   
    

    logic pe_en;                      
    logic pe_w_en;                    
    logic [71:0] pe_active_left;      
    logic [7:0] pe_in_weight_above;   
    logic [15:0] pe_out_sum_final;    
    

    assign pe_active_left = input_reg;
    assign pe_in_weight_above = weight_reg;
    

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_state <= IDLE;
        end else begin
            current_state <= next_state;
        end
    end
    

    always_comb begin
        next_state = current_state;
        
        case (current_state)
            IDLE: begin
                if (weight_data_ready) begin
                    next_state = LOAD_WEIGHT;
                end
            end
            
            LOAD_WEIGHT: begin
                if (weight_loaded && input_data_ready) begin
                    next_state = PIPELINE_PROCESSING;
                end
            end
            
            PIPELINE_PROCESSING: begin
                if (data_counter >= MAX_DATA_COUNT) begin
                    next_state = FINISH_PROCESSING;
                end
            end
            
            FINISH_PROCESSING: begin
                if (processing_done) begin
                    next_state = OUTPUT_RESULT;
                end
            end
            
            OUTPUT_RESULT: begin
                if (output_counter >= OUTPUT_CYCLES) begin
                    next_state = IDLE;
                end
            end
            
            default: next_state = IDLE;
        endcase
    end
    

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            weight_reg <= 8'b0;
            input_reg <= 72'b0;
            output_reg <= 16'b0;
            
            weight_counter <= 4'b0;
            weight_loaded <= 1'b0;
            processing_done <= 1'b0;
            
            data_counter <= 8'b0;
            output_counter <= 8'b0;
            
            output_start <= 1'b0;
            input_start <= 1'b0;
            weight_start <= 1'b0;
            output_data <= 16'b0;
            

            pe_en <= 1'b0;
            pe_w_en <= 1'b0;
        end else begin
            case (current_state)
                IDLE: begin
                    weight_counter <= 4'b0;
                    weight_loaded <= 1'b0;
                    processing_done <= 1'b0;
                    
                    data_counter <= 8'b0;
                    output_counter <= 8'b0;
                    
                    output_start <= 1'b0;
                    input_start <= 1'b0;
                    weight_start <= 1'b0;
                    
                    pe_en <= 1'b0;
                    pe_w_en <= 1'b0;
                end
                
                LOAD_WEIGHT: begin
                    if (!weight_loaded) begin
                        weight_start <= 1'b1;
                        pe_en <= 1'b1;
                        pe_w_en <= 1'b1;


                        if (weight_data_ready) begin
                            weight_reg <= weight_data;
                            weight_counter <= weight_counter + 1'b1;

                            if (weight_counter == (MAX_WEIGHTS)) begin
                                weight_loaded <= 1'b1;
                            end
                        end
                    end else begin
                        weight_start <= 1'b0;
                        pe_en <= 1'b1;
                        pe_w_en <= 1'b0;
                    end
                end

                
                PIPELINE_PROCESSING: begin
                    weight_start <= 1'b0;   
                    pe_w_en <= 1'b0;        
                    input_start <= 1'b1;    
                    pe_en <= 1'b1;          
                    

                    if (input_data_ready) begin
                        input_reg <= input_data;        
                        data_counter <= data_counter + 1'b1; 
                        output_reg <= pe_out_sum_final;
                        
                        if (data_counter > 11) begin
                            output_start <= 1'b1;
                            output_data <= pe_out_sum_final;
                        end
                    end
                end
                
                FINISH_PROCESSING: begin
                    input_start <= 1'b0;    
                    output_data <= pe_out_sum_final;
                    

                    output_counter <= output_counter + 1'b1;
                    
                    if (output_counter >= 2) begin  
                        processing_done <= 1'b1;
                        output_start <= 1'b0;  
                    end
                end
                
                OUTPUT_RESULT: begin

                    pe_en <= 1'b0;
                    
                    output_counter <= output_counter + 1'b1; 
                end
                
                default: begin

                end
            endcase
        end
    end
    

    PE_array #(
        .num1(9),
        .num2(1)
    ) pe_array_inst (
        .CLK(clk),
        .RESET(rst_n),
        .EN(pe_en),
        .W_EN(pe_w_en),
        .active_left(pe_active_left),
        .in_weight_above(pe_in_weight_above),
        .out_sum_final(pe_out_sum_final),
        .out_weight_final()  
    );

endmodule
