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
vlog +acc -incr ../../rtl/output_buffer.sv
vlog +acc -incr ../../rtl/weight_buffer.sv
vlog +acc -incr ../../rtl/top.sv
vlog +acc -incr ../../rtl/ctrl.sv
vlog +acc -incr top_tb.sv

#Run Simulator 
vsim -voptargs=+acc -t ps -lib work top_tb


do wave_top.do


# 运行仿真
run -all

