# intentionally incorrect: set_false_path used as a blanket "make timing pass"
# on a functional path. This hides a real setup violation. Legal false paths
# are only CDC crossings (already synchronized) or known-static signals.
set_false_path -from [get_ports data_in] -to [get_ports data_out]

# intentionally incorrect: set_max_delay as a crutch across the whole block.
set_max_delay 3.0 -from [all_inputs] -to [all_outputs]
