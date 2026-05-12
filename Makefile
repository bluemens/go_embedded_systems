# Top-level Makefile — orchestrates HW build, SW build, simulation, SD imaging.

NUM_ENGINES     ?= 8
SIMS_PER_ENGINE ?= 25

.PHONY: all hw sw sim sim_flood_fill sim_rollout sim_mcts \
        vectors ab_test self_test sd_image flash_sd clean help

help:
	@echo "Targets:"
	@echo "  make hw                Quartus full compile (NUM_ENGINES=$(NUM_ENGINES))"
	@echo "  make hw FAST=1         Same but NUM_ENGINES=1 (fast bring-up build)"
	@echo "  make sw                Cross-compile userspace binaries for DE1-SoC"
	@echo "  make sim               Run all SystemVerilog testbenches headless"
	@echo "  make sim_flood_fill    Only the novel flood_fill module"
	@echo "  make sim_rollout       Only the rollout_engine module"
	@echo "  make sim_mcts          Only the top-level mcts_accel module"
	@echo "  make vectors           Regenerate Python golden vectors"
	@echo "  make sd_image          Build bootable sdcard.img"
	@echo "  make flash_sd          Write sdcard.img → SD card (needs SD_DEV)"

ifeq ($(FAST),1)
NUM_ENGINES := 1
endif

all: hw sw sim

hw:
	$(MAKE) -C hw NUM_ENGINES=$(NUM_ENGINES) SIMS_PER_ENGINE=$(SIMS_PER_ENGINE)

sw:
	$(MAKE) -C sw

sim sim_flood_fill sim_rollout sim_mcts vectors:
	$(MAKE) -C sim $@

ab_test: sw
	@echo "Run on target: ./sw/go_self_test ab 1000"

self_test: sw
	@echo "Run on target: ./sw/go_self_test selftest"

sd_image:
	./scripts/build_sd_image.sh

flash_sd:
	./scripts/flash_sd.sh

clean:
	$(MAKE) -C sim clean
	$(MAKE) -C sw clean || true
