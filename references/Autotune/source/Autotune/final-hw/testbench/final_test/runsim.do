set QSYS_SIMDIR ../../my_fft/simulation
source $QSYS_SIMDIR/mentor/msim_setup.tcl

dev_com
com

vlog ../my_fft.v ../fft_ifft_final.sv tb_fft_ifft_peak.sv

set TOP_LEVEL_NAME testbench
elab

do waveformat.do
run 1000 us