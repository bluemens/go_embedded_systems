onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate /top_tb/clk
add wave -noupdate /top_tb/data_in
add wave -noupdate /top_tb/write
add wave -noupdate /top_tb/read
add wave -noupdate /top_tb/addr
add wave -noupdate /top_tb/chipselect
add wave -noupdate /top_tb/data_out
add wave -noupdate /top_tb/test_input
add wave -noupdate /top_tb/test_weight
add wave -noupdate /top_tb/test_output
add wave -noupdate /top_tb/expected_output
add wave -noupdate /top_tb/sum
add wave -noupdate /top_tb/output_counter
add wave -noupdate /top_tb/cycle_counter
add wave -noupdate -divider {input buffer}
add wave -noupdate /top_tb/DUT/input_buffer_inst/clk
add wave -noupdate /top_tb/DUT/input_buffer_inst/rst_n
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in
add wave -noupdate /top_tb/DUT/input_buffer_inst/rd_en
add wave -noupdate /top_tb/DUT/input_buffer_inst/wr_en
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_out
add wave -noupdate /top_tb/DUT/input_buffer_inst/ready
add wave -noupdate /top_tb/DUT/input_buffer_inst/done
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in_bank0
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in_bank1
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in_bank2
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in_bank3
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in_bank4
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in_bank5
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in_bank6
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in_bank7
add wave -noupdate /top_tb/DUT/input_buffer_inst/data_in_bank8
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_wr
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_rd
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_bank0
add wave -noupdate /top_tb/DUT/input_buffer_inst/counter
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_bank1
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_bank2
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_bank3
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_bank4
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_bank5
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_bank6
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_bank7
add wave -noupdate /top_tb/DUT/input_buffer_inst/index_bank8
add wave -noupdate -color {Blue Violet} /top_tb/DUT/weight_buffer_inst/data_out
add wave -noupdate -color {Blue Violet} /top_tb/DUT/weight_buffer_inst/rd_addr
add wave -noupdate -color {Blue Violet} /top_tb/DUT/weight_buffer_inst/weight_bank0
add wave -noupdate -divider ctrl
add wave -noupdate /top_tb/DUT/ctrl_inst/clk
add wave -noupdate /top_tb/DUT/ctrl_inst/rst_n
add wave -noupdate /top_tb/DUT/ctrl_inst/input_data_ready
add wave -noupdate /top_tb/DUT/ctrl_inst/weight_data_ready
add wave -noupdate /top_tb/DUT/ctrl_inst/weight_data
add wave -noupdate /top_tb/DUT/ctrl_inst/input_start
add wave -noupdate /top_tb/DUT/ctrl_inst/weight_start
add wave -noupdate /top_tb/DUT/ctrl_inst/output_data
add wave -noupdate -color Yellow /top_tb/DUT/ctrl_inst/output_start
add wave -noupdate /top_tb/DUT/ctrl_inst/current_state
add wave -noupdate /top_tb/DUT/ctrl_inst/next_state
add wave -noupdate -color {Blue Violet} /top_tb/DUT/ctrl_inst/weight_reg
add wave -noupdate /top_tb/DUT/ctrl_inst/input_reg
add wave -noupdate /top_tb/DUT/ctrl_inst/output_reg
add wave -noupdate -color Cyan -radix unsigned /top_tb/DUT/ctrl_inst/weight_counter
add wave -noupdate -color Cyan /top_tb/DUT/ctrl_inst/weight_loaded
add wave -noupdate /top_tb/DUT/ctrl_inst/processing_done
add wave -noupdate -color Yellow /top_tb/DUT/ctrl_inst/input_data
add wave -noupdate -color Gold /top_tb/DUT/ctrl_inst/output_start
add wave -noupdate -color Coral -itemcolor Yellow -radix unsigned /top_tb/DUT/ctrl_inst/data_counter
add wave -noupdate -itemcolor Yellow -radix unsigned /top_tb/DUT/ctrl_inst/output_counter
add wave -noupdate /top_tb/DUT/output_buffer_inst/wr_en
add wave -noupdate /top_tb/DUT/output_buffer_inst/output_bank0
add wave -noupdate /top_tb/DUT/output_buffer_inst/wr_addr
add wave -noupdate /top_tb/DUT/output_buffer_inst/ready
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_en
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_w_en
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_active_left
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_in_weight_above
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_out_sum_final
add wave -noupdate -divider ctrl
add wave -noupdate /top_tb/DUT/ctrl_inst/clk
add wave -noupdate /top_tb/DUT/ctrl_inst/rst_n
add wave -noupdate /top_tb/DUT/ctrl_inst/input_data_ready
add wave -noupdate /top_tb/DUT/ctrl_inst/weight_data_ready
add wave -noupdate /top_tb/DUT/ctrl_inst/weight_data
add wave -noupdate /top_tb/DUT/ctrl_inst/weight_start
add wave -noupdate /top_tb/DUT/ctrl_inst/current_state
add wave -noupdate /top_tb/DUT/ctrl_inst/next_state
add wave -noupdate -color Cyan -radix unsigned /top_tb/DUT/ctrl_inst/weight_counter
add wave -noupdate -color Cyan /top_tb/DUT/ctrl_inst/weight_loaded
add wave -noupdate -color Green /top_tb/DUT/ctrl_inst/weight_reg
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_en
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_w_en
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_in_weight_above
add wave -noupdate -color {Green Yellow} /top_tb/DUT/ctrl_inst/pe_array_inst/weight_connections
add wave -noupdate -divider {PE ARRAY}
add wave -noupdate -color {Blue Violet} /top_tb/DUT/ctrl_inst/pe_array_inst/weight_connections
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_array_inst/CLK
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_array_inst/RESET
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_array_inst/EN
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_array_inst/W_EN
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_array_inst/active_left
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_array_inst/in_weight_above
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_array_inst/out_weight_final
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_array_inst/out_sum_final
add wave -noupdate /top_tb/DUT/ctrl_inst/pe_array_inst/sum_connections
add wave -noupdate -divider {weight buffer}
add wave -noupdate /top_tb/DUT/weight_buffer_inst/clk
add wave -noupdate /top_tb/DUT/weight_buffer_inst/rst_n
add wave -noupdate /top_tb/DUT/weight_buffer_inst/data_in
add wave -noupdate /top_tb/DUT/weight_buffer_inst/wr_en
add wave -noupdate /top_tb/DUT/weight_buffer_inst/rd_en
add wave -noupdate /top_tb/DUT/weight_buffer_inst/data_out
add wave -noupdate /top_tb/DUT/weight_buffer_inst/ready
add wave -noupdate /top_tb/DUT/weight_buffer_inst/weight_bank0
add wave -noupdate /top_tb/DUT/weight_buffer_inst/wr_addr
add wave -noupdate /top_tb/DUT/weight_buffer_inst/rd_addr
add wave -noupdate -divider {output buffer}
add wave -noupdate /top_tb/DUT/output_buffer_inst/clk
add wave -noupdate /top_tb/DUT/output_buffer_inst/rst_n
add wave -noupdate /top_tb/DUT/output_buffer_inst/data_in
add wave -noupdate /top_tb/DUT/output_buffer_inst/rd_en
add wave -noupdate /top_tb/DUT/output_buffer_inst/data_out
add wave -noupdate /top_tb/DUT/output_buffer_inst/ready
add wave -noupdate /top_tb/DUT/output_buffer_inst/wr_en
add wave -noupdate /top_tb/DUT/output_buffer_inst/output_bank0
add wave -noupdate /top_tb/DUT/output_buffer_inst/wr_addr
add wave -noupdate /top_tb/DUT/output_buffer_inst/rd_addr
add wave -noupdate /top_tb/DUT/weight_buffer_inst/rd_en
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {948170 ps} 0} {{Cursor 2} {5853498 ps} 0}
quietly wave cursor active 2
configure wave -namecolwidth 365
configure wave -valuecolwidth 100
configure wave -justifyvalue left
configure wave -signalnamewidth 0
configure wave -snapdistance 10
configure wave -datasetprefix 0
configure wave -rowmargin 4
configure wave -childrowmargin 2
configure wave -gridoffset 0
configure wave -gridperiod 1
configure wave -griddelta 40
configure wave -timeline 0
configure wave -timelineunits ps
update
WaveRestoreZoom {8476137 ps} {8844703 ps}
