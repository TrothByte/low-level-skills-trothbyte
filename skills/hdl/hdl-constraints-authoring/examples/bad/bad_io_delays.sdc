# intentionally incorrect: input/output delay values that do not match the
# real external timing. The spec says 2.0 ns, the constraint says 8.0 — the
# timing analysis is not testing the real interface.
create_clock -name clk -period 10.000 [get_ports clk]

# Wrong: output delay larger than the clock period and no relation to real
# external logic delay; makes the report artificially green.
set_output_delay 8.0 -clock clk [get_ports data_out]

# Wrong: no set_input_delay at all for data_in — unconstrained inputs are
# NOT analyzed, so the report silently skips the input-to-reg path.
#   (missing) set_input_delay 2.0 -clock clk [get_ports data_in]
