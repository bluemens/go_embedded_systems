##################################################
#  Modelsim do file to run simuilation for Control
##################################################
#Setup
vlib work 
vmap work work

#Include Netlist and Testbench
vlog +acc -incr ../../rtl/dffr.v
vlog +acc -incr ../../rtl/PE_array.v
vlog +acc -incr ../../rtl/PE_row.v
vlog +acc -incr ../../rtl/PE.v
vlog +acc -incr ../../rtl/input_buffer.sv
vlog +acc -incr inputbuffer_PE_tb.sv

#Run Simulator 
vsim -voptargs=+acc -t ps -lib work testbench


do wave_inputbuffer_PE.do


# 运行仿真
run -all

