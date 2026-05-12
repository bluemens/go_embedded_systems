# add_mcts_accel.tcl — add the mcts_accel component to soc_system.qsys.
#
# Run once per fresh checkout:
#   cd hw && qsys-script --script=add_mcts_accel.tcl --quartus-project=soc_system
#
# Then regenerate the system:
#   qsys-generate soc_system.qsys --synthesis=VERILOG
#
# The component lives at hw/ip/mcts_accel/ and is loaded from its _hw.tcl by
# qsys-script via the IP search path set below.

load_system soc_system.qsys

# Make Qsys find the new IP at hw/ip/mcts_accel/
set_project_property IP_SEARCH_PATHS "ip/**/*"

# Add the component instance. Parameters use defaults from mcts_accel_hw.tcl.
add_instance mcts_accel_0 mcts_accel

# Connections:
#   - Avalon slave attached to the LW HPS-to-FPGA bridge (via hps_0)
#   - Clock + reset from clk_0 (the same 50 MHz source already driving the
#     LW bridge — see existing soc_system.qsys connection
#     clk_0.clk → hps_0.h2f_lw_axi_clock)
#   Pattern from references/FPGA-MPC/test_final_4840/soc_system.qsys.
add_connection hps_0.h2f_lw_axi_master/mcts_accel_0.avalon_slave_0
add_connection clk_0.clk/mcts_accel_0.clock
add_connection clk_0.clk_reset/mcts_accel_0.reset

# Base address. The LW bridge is mmap'd at 0xFF200000 on the HPS side; the
# in-bridge address of mcts_accel is 0x20 per design-doc §8.1 / §7.5.8.
set_connection_parameter_value \
    hps_0.h2f_lw_axi_master/mcts_accel_0.avalon_slave_0 baseAddress "0x0020"

save_system soc_system.qsys
puts "mcts_accel_0 added to soc_system.qsys"
