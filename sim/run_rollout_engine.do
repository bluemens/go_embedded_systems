if {[file exists work]} { vdel -lib work -all }
vlib work

set HW ../hw/ip/mcts_accel
vlog -sv $HW/mcts_accel_pkg.sv
vlog -sv $HW/flood_fill.sv
vlog -sv $HW/lfsr16.sv
vlog -sv $HW/rollout_engine.sv
vlog -sv rollout_engine_tb.sv

vsim -t 1ps work.rollout_engine_tb
if {[batch_mode]} {
    run -all
    quit -f
} else {
    add wave -position end sim:/rollout_engine_tb/dut/state
    add wave -position end sim:/rollout_engine_tb/dut/black
    add wave -position end sim:/rollout_engine_tb/dut/white
    add wave -position end sim:/rollout_engine_tb/dut/move_count
    add wave -position end sim:/rollout_engine_tb/dut/sim_first_cell
    run -all
}
