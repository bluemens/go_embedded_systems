onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate /testbench/clk
add wave -noupdate /testbench/rst_n
add wave -noupdate /testbench/en
add wave -noupdate /testbench/w_en
add wave -noupdate /testbench/data_in
add wave -noupdate /testbench/rd_en
add wave -noupdate /testbench/wr_en
add wave -noupdate /testbench/data_out
add wave -noupdate /testbench/buffer_done
add wave -noupdate /testbench/active_left
add wave -noupdate /testbench/in_weight_above
add wave -noupdate /testbench/out_weight_final
add wave -noupdate /testbench/out_sum_final
add wave -noupdate /testbench/errors
add wave -noupdate /testbench/total_tests
add wave -noupdate /testbench/input_values
add wave -noupdate /testbench/output_values
add wave -noupdate /testbench/output_index
add wave -noupdate -divider {New Divider}
add wave -noupdate /testbench/input_buffer_inst/data_in_bank0
add wave -noupdate /testbench/input_buffer_inst/data_in_bank1
add wave -noupdate /testbench/input_buffer_inst/data_in_bank2
add wave -noupdate /testbench/input_buffer_inst/data_in_bank3
add wave -noupdate /testbench/input_buffer_inst/data_in_bank4
add wave -noupdate /testbench/input_buffer_inst/data_in_bank5
add wave -noupdate /testbench/input_buffer_inst/data_in_bank6
add wave -noupdate /testbench/input_buffer_inst/data_in_bank7
add wave -noupdate /testbench/input_buffer_inst/data_in_bank8
add wave -noupdate /testbench/input_buffer_inst/index_wr
add wave -noupdate /testbench/input_buffer_inst/index_rd
add wave -noupdate /testbench/input_buffer_inst/index_bank0
add wave -noupdate /testbench/input_buffer_inst/counter
add wave -noupdate /testbench/input_buffer_inst/index_bank1
add wave -noupdate /testbench/input_buffer_inst/index_bank2
add wave -noupdate /testbench/input_buffer_inst/index_bank3
add wave -noupdate /testbench/input_buffer_inst/index_bank4
add wave -noupdate /testbench/input_buffer_inst/index_bank5
add wave -noupdate /testbench/input_buffer_inst/index_bank6
add wave -noupdate /testbench/input_buffer_inst/index_bank7
add wave -noupdate /testbench/input_buffer_inst/index_bank8
add wave -noupdate -divider {New Divider}
add wave -noupdate /testbench/pe_array_inst/CLK
add wave -noupdate /testbench/pe_array_inst/RESET
add wave -noupdate /testbench/pe_array_inst/EN
add wave -noupdate /testbench/pe_array_inst/W_EN
add wave -noupdate /testbench/pe_array_inst/active_left
add wave -noupdate /testbench/pe_array_inst/in_weight_above
add wave -noupdate /testbench/pe_array_inst/out_weight_final
add wave -noupdate /testbench/pe_array_inst/out_sum_final
add wave -noupdate /testbench/pe_array_inst/weight_connections
add wave -noupdate /testbench/pe_array_inst/sum_connections
add wave -noupdate -divider {New Divider}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/CLK}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/RESET}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/EN}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/W_EN}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/active_left}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/input_next}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/active_right}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/in_sum}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/out_sum}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/in_weight_above}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/out_weight_below}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/weight_next}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/weight_q}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/input_q}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/sum_next}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/sum_q}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/out_next}
add wave -noupdate {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/out_q}
add wave -noupdate -radix hexadecimal {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/mul_result}
add wave -noupdate -radix hexadecimal {/testbench/pe_array_inst/label[0]/PE_row_unit/label[0]/genblk1/PE_unit/add_result}
add wave -noupdate -divider {New Divider}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/CLK}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/RESET}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/EN}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/W_EN}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/active_left}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/active_right}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/in_sum}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/out_sum}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/in_weight_above}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/out_weight_below}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/weight_next}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/weight_q}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/input_next}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/input_q}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/sum_next}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/sum_q}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/out_next}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/out_q}
add wave -noupdate {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/mul_result}
add wave -noupdate -radix hexadecimal {/testbench/pe_array_inst/label[1]/PE_row_unit/label[0]/genblk1/PE_unit/add_result}
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {3073982 ps} 0}
quietly wave cursor active 1
configure wave -namecolwidth 538
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
WaveRestoreZoom {0 ps} {5570250 ps}
