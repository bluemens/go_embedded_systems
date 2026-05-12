# ModelSim script for flood_fill testbench.
# Run from sim/ as: vsim -c -do run_flood_fill.do
# Or with waves:    vsim -do run_flood_fill.do

if {[file exists work]} { vdel -lib work -all }
vlib work

set HW ../hw/ip/mcts_accel
vlog -sv $HW/mcts_accel_pkg.sv
vlog -sv $HW/flood_fill.sv
vlog -sv flood_fill_tb.sv

vsim -t 1ps work.flood_fill_tb

# Optional waves
if {[batch_mode]} {
    run -all
    quit -f
} else {
    add wave -position end sim:/flood_fill_tb/*
    add wave -position end sim:/flood_fill_tb/dut/*
    run -all
}
