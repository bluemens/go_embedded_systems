# sim/Makefile — runs ModelSim testbenches headless and diffs vs Python golden.

VSIM    := vsim
PYTHON  := python3
GOLDEN  := ../sw/golden
VECDIR  := vectors

.PHONY: all vectors sim_flood_fill sim_rollout sim_mcts clean

all: sim_flood_fill sim_rollout sim_mcts

# Regenerate all golden vectors from go_model.py
vectors:
	$(PYTHON) $(GOLDEN)/gen_vectors.py --what all

# Each sim target depends on its vectors file.
$(VECDIR)/ff_vectors.txt: $(GOLDEN)/go_model.py $(GOLDEN)/gen_vectors.py
	$(PYTHON) $(GOLDEN)/gen_vectors.py --what ff

$(VECDIR)/re_vectors.txt: $(GOLDEN)/go_model.py $(GOLDEN)/gen_vectors.py
	$(PYTHON) $(GOLDEN)/gen_vectors.py --what re

$(VECDIR)/mc_vectors.txt: $(GOLDEN)/go_model.py $(GOLDEN)/gen_vectors.py
	$(PYTHON) $(GOLDEN)/gen_vectors.py --what mc

sim_flood_fill: $(VECDIR)/ff_vectors.txt
	$(VSIM) -c -do run_flood_fill.do
	$(PYTHON) $(GOLDEN)/compare.py $(VECDIR)/ff_vectors.txt flood_fill_dut.txt \
	    --field-start 3

sim_rollout: $(VECDIR)/re_vectors.txt
	$(VSIM) -c -do run_rollout_engine.do
	$(PYTHON) $(GOLDEN)/compare.py $(VECDIR)/re_vectors.txt rollout_engine_dut.txt \
	    --field-start 6

sim_mcts: $(VECDIR)/mc_vectors.txt
	$(VSIM) -c -do run_mcts_accel.do
	$(PYTHON) $(GOLDEN)/compare.py $(VECDIR)/mc_vectors.txt mcts_accel_dut.txt \
	    --field-start 7

clean:
	rm -rf work transcript *.wlf *_dut.txt vsim.wlf
