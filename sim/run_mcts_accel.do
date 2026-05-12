if {[file exists work]} { vdel -lib work -all }
vlib work

set HW ../hw/ip/mcts_accel
vlog -sv $HW/mcts_accel_pkg.sv
vlog -sv $HW/flood_fill.sv
vlog -sv $HW/lfsr16.sv
vlog -sv $HW/rollout_engine.sv
vlog -sv $HW/mcts_accel.sv
vlog -sv mcts_accel_tb.sv

vsim -t 1ps work.mcts_accel_tb
if {[batch_mode]} {
    run -all
    quit -f
} else {
    add wave -position end sim:/mcts_accel_tb/dut/state
    run -all
}
