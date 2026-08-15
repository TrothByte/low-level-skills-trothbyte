# Correct: full, self-consistent SDC for a single-clock design.
# One clock per source, defined on the actual clock port, with the real
# period (10 ns = 100 MHz).

create_clock -name clk -period 10.000 -waveform {0 5} [get_ports clk]

# External interface timing: 2.0 ns input delay (external FF-to-FF path +
# board delay), 3.0 ns output delay (board trace + downstream setup).
set_input_delay  2.0 -clock clk [get_ports data_in]
set_output_delay 3.0 -clock clk [get_ports data_out]

# Correct exception: false path for a CDC input that is ALREADY
# synchronized in RTL (see hdl-cdc-audit good/cdc_two_ff_sync.v).
set_false_path -from [get_ports async_in]

# Multi-cycle path with documented reason: the accumulator only needs the
# result every 4 cycles.
set_multicycle_path 4 -setup -from [get_ports data_in] -to [get_ports acc]

# Sanity: verify all objects exist in the synthesized netlist.
check_timing -verbose
