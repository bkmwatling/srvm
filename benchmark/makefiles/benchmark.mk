.PHONY: benchmark
benchmark: benchmark-all benchmark-sl

.PHONY: benchmark-all
benchmark-all: benchmark-all-full benchmark-all-partial

.PHONY: benchmark-sl
benchmark-sl: benchmark-sl-full benchmark-sl-partial

.PHONY: benchmark-all-full
benchmark-all-full: $(BENCHMARK_ALL_FULL)

.PHONY: benchmark-all-partial
benchmark-all-partial: $(BENCHMARK_ALL_PARTIAL)

.PHONY: benchmark-sl-full
benchmark-sl-full: $(BENCHMARK_SL_FULL)

.PHONY: benchmark-sl-partial
benchmark-sl-partial: $(BENCHMARK_SL_PARTIAL)

.PRECIOUS: $(BENCHMARK_DIR)/all-%.jsonl
.ONESHELL:
$(BENCHMARK_DIR)/sl-%.jsonl: \
		$(SL_REGEX_DATASET) | $(BENCHMARK_DIR) $(VENV)
	@echo "make $@"
	@args=($(subst -, ,$*))
	@$(PYTHON) src/benchmark.py \
		--regex-type sl \
		--matching_type $${args[0]} \
		--construction $${args[1]} \
		--scheduler $${args[2]} \
		--memo-scheme $${args[3]} \
		$< $@ \
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log

.PRECIOUS: $(BENCHMARK_DIR)/sl-%.jsonl
.ONESHELL:
$(BENCHMARK_DIR)/all-%.jsonl: \
		$(ALL_REGEX_DATASET) | $(BENCHMARK_DIR) $(VENV)
	@echo "make $@"
	@args=($(subst -, ,$*))
	@$(PYTHON) src/benchmark.py \
		--regex-type all \
		--matching_type $${args[0]} \
		--construction $${args[1]} \
		--scheduler $${args[2]} \
		--memo-scheme $${args[3]} \
		$< $@ \
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log