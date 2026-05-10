onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate /testbench/clk
add wave -noupdate /testbench/reset_n
add wave -noupdate /testbench/sink_valid
add wave -noupdate /testbench/sink_ready
add wave -noupdate /testbench/sink_sop
add wave -noupdate /testbench/sink_eop
add wave -noupdate /testbench/sink_real
add wave -noupdate /testbench/sink_imag
add wave -noupdate /testbench/fftpts_in
add wave -noupdate /testbench/fftpts_out
add wave -noupdate /testbench/source_ready
add wave -noupdate /testbench/source_valid
add wave -noupdate /testbench/source_sop
add wave -noupdate /testbench/source_eop
add wave -noupdate /testbench/source_real
add wave -noupdate /testbench/source_imag
add wave -noupdate /testbench/source_mag
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {3 ns} 0}
quietly wave cursor active 1
configure wave -namecolwidth 223
configure wave -valuecolwidth 89
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
configure wave -timelineunits us
update
WaveRestoreZoom {0 ns} {12 ns}


