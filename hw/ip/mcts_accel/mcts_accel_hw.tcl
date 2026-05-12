# mcts_accel_hw.tcl — Qsys component descriptor.
# Pattern adapted from references/FPGA-MPC/test_final_4840/admm_solver_hw.tcl.

package require -exact qsys 16.1

set_module_property DESCRIPTION ""
set_module_property NAME mcts_accel
set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property AUTHOR ""
set_module_property DISPLAY_NAME MCTS_ACCEL
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property REPORT_HIERARCHY false
set_module_assignment embeddedsw.dts.vendor "csee4840"
set_module_assignment embeddedsw.dts.name "mcts_accel"
set_module_assignment embeddedsw.dts.group "mcts"

# Filesets
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL mcts_accel
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property QUARTUS_SYNTH ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file mcts_accel_pkg.sv  SYSTEM_VERILOG PATH mcts_accel_pkg.sv
add_fileset_file flood_fill.sv      SYSTEM_VERILOG PATH flood_fill.sv
add_fileset_file lfsr16.sv          SYSTEM_VERILOG PATH lfsr16.sv
add_fileset_file rollout_engine.sv  SYSTEM_VERILOG PATH rollout_engine.sv
add_fileset_file mcts_accel.sv      SYSTEM_VERILOG PATH mcts_accel.sv TOP_LEVEL_FILE

# Parameters
add_parameter NUM_ENGINES INTEGER 8 ""
set_parameter_property NUM_ENGINES DEFAULT_VALUE 8
set_parameter_property NUM_ENGINES DISPLAY_NAME NUM_ENGINES
set_parameter_property NUM_ENGINES TYPE INTEGER
set_parameter_property NUM_ENGINES ALLOWED_RANGES {1 2 4 8 16}
set_parameter_property NUM_ENGINES HDL_PARAMETER true

add_parameter SIMS_PER_ENGINE INTEGER 25 ""
set_parameter_property SIMS_PER_ENGINE DEFAULT_VALUE 25
set_parameter_property SIMS_PER_ENGINE DISPLAY_NAME SIMS_PER_ENGINE
set_parameter_property SIMS_PER_ENGINE TYPE INTEGER
set_parameter_property SIMS_PER_ENGINE ALLOWED_RANGES 1:65535
set_parameter_property SIMS_PER_ENGINE HDL_PARAMETER true

# Clock interface
add_interface clock clock end
set_interface_property clock clockRate 0
add_interface_port clock clk clk Input 1

# Reset interface
add_interface reset reset end
set_interface_property reset associatedClock clock
set_interface_property reset synchronousEdges DEASSERT
add_interface_port reset reset_n reset_n Input 1

# Avalon-MM slave (byte addresses, 32-bit data)
add_interface avalon_slave_0 avalon end
set_interface_property avalon_slave_0 addressUnits SYMBOLS
set_interface_property avalon_slave_0 associatedClock clock
set_interface_property avalon_slave_0 associatedReset reset
set_interface_property avalon_slave_0 bitsPerSymbol 8
set_interface_property avalon_slave_0 explicitAddressSpan 256
set_interface_property avalon_slave_0 readLatency 1
set_interface_property avalon_slave_0 readWaitTime 0
set_interface_property avalon_slave_0 writeWaitTime 0

add_interface_port avalon_slave_0 avs_address    address    Input 8
add_interface_port avalon_slave_0 avs_read       read       Input 1
add_interface_port avalon_slave_0 avs_write      write      Input 1
add_interface_port avalon_slave_0 avs_writedata  writedata  Input 32
add_interface_port avalon_slave_0 avs_readdata   readdata   Output 32
add_interface_port avalon_slave_0 avs_chipselect chipselect Input 1
