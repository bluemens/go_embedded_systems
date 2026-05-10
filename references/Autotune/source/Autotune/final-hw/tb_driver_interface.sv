`timescale 1ns/1ps

module driver_interface_tb();
    // Parameters
    parameter DATA_SIZE = 28;
    parameter CLK_PERIOD = 20; // 50MHz clock

    // Test signals
    logic clk;
    logic rst;
    logic chipselect;
    logic address;
    logic read;
    logic source_valid;
    logic [DATA_SIZE-1:0] source_data;
    logic source_ready;
    logic [31:0] read_data;
    logic irq;

    // Instantiate the driver interface
    driver_interface #(
        .DATA_SIZE(DATA_SIZE)
    ) dut (
        .clk(clk),
        .rst(rst),
        .chipselect(chipselect),
        .address(address),
        .read(read),
        .source_valid(source_valid),
        .source_data(source_data),
        .source_ready(source_ready),
        .read_data(read_data),
        .irq(irq)
    );

    // Clock generation
    initial begin
        clk = 0;
        forever #(CLK_PERIOD/2) clk = ~clk;
    end

    // Test stimulus
    initial begin
        // Initialize signals
        rst = 1;
        chipselect = 0;
        address = 0;
        read = 0;
        source_valid = 0;
        source_data = 0;

        // Reset sequence
        #(CLK_PERIOD*2);
        rst = 0;
        #(CLK_PERIOD*2);

        // Test Case 1: Basic data transfer and read
        // Send data
        source_valid = 1;
        source_data = 28'h1234567;
        #(CLK_PERIOD);
        source_valid = 0;
        #(CLK_PERIOD*2);

        // Read the data
        chipselect = 1;
        read = 1;
        address = 0;
        #(CLK_PERIOD);
        chipselect = 0;
        read = 0;
        #(CLK_PERIOD*2);

        // Test Case 2: Multiple data transfers
        // First data transfer
        source_valid = 1;
        source_data = 28'hABCDEF0;
        #(CLK_PERIOD);
        source_valid = 0;
        #(CLK_PERIOD*2);

        // Read first data
        chipselect = 1;
        read = 1;
        address = 0;
        #(CLK_PERIOD);
        chipselect = 0;
        read = 0;
        #(CLK_PERIOD*2);

        // Second data transfer
        source_valid = 1;
        source_data = 28'h9876543;
        #(CLK_PERIOD);
        source_valid = 0;
        #(CLK_PERIOD*2);

        // Read second data
        chipselect = 1;
        read = 1;
        address = 0;
        #(CLK_PERIOD);
        chipselect = 0;
        read = 0;
        #(CLK_PERIOD*2);

        // Test Case 3: Continuous data transfer
        source_valid = 1;
        source_data = 28'h1111111;
        #(CLK_PERIOD);
        source_data = 28'h2222222;
        #(CLK_PERIOD);
        source_data = 28'h3333333;
        #(CLK_PERIOD);
        source_valid = 0;
        #(CLK_PERIOD*2);

        // Read all data
        chipselect = 1;
        read = 1;
        address = 0;
        #(CLK_PERIOD);  // Read first value
        #(CLK_PERIOD);  // Read second value
        #(CLK_PERIOD);  // Read third value
        chipselect = 0;
        read = 0;
        #(CLK_PERIOD*2);

        // Test Case 4: Verify source_ready is always 1
        #(CLK_PERIOD*5);

        // End simulation
        #(CLK_PERIOD*20);  // Increased from 10 to 20
        $finish;
    end

    // Monitor and display results
    initial begin
        $monitor("Time=%0t rst=%b chipselect=%b address=%b read=%b valid=%b data=%h ready=%b read_data=%h irq=%b",
                 $time, rst, chipselect, address, read, source_valid, source_data, source_ready, read_data, irq);
    end

    // Add waveform dumping
    initial begin
        $dumpfile("driver_interface_tb.vcd");
        $dumpvars(0, driver_interface_tb);
    end

    // Assertions
    property source_ready_always_high;
        @(posedge clk) source_ready == 1'b1;
    endproperty
    assert property (source_ready_always_high) else $error("source_ready should always be 1");

    property irq_always_low;
        @(posedge clk) irq == 1'b0;
    endproperty
    assert property (irq_always_low) else $error("irq should always be 0");

endmodule
