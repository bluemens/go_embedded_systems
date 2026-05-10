set QSYS_SIMDIR ../../my_fft/simulation
source $QSYS_SIMDIR/mentor/msim_setup.tcl

dev_com
com

vlog ../../modules/my_fft.v my_fft_tb.v ../../modules/data2mag.sv

set TOP_LEVEL_NAME testbench
elab

do waveformat.do
run 500 us