SIM := build/sim
CAPTURE := captures/04_stm32_status_pin21_up_down_sequence.log
ESPHOME ?= esphome

.PHONY: test config firmware clean

test: $(SIM)
	./$(SIM) $(CAPTURE)

$(SIM): test/sim.cpp esphome/components/brunt/brunt_cover.cpp esphome/components/brunt/brunt_cover.h $(wildcard test/mock/esphome/*/*.h test/mock/esphome/*/*/*.h)
	@mkdir -p build
	g++ -std=c++17 -Wall -Wextra -g -fsanitize=address,undefined \
	  -Itest/mock -Iesphome/components/brunt \
	  test/sim.cpp esphome/components/brunt/brunt_cover.cpp -o $@

config:
	cd esphome && $(ESPHOME) config brunt-blind.yaml

firmware:
	cd esphome && $(ESPHOME) compile brunt-blind.yaml

clean:
	rm -rf build esphome/.esphome
