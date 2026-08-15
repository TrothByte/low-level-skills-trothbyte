// intentionally incorrect: bad SDC constraint authoring.
// Clock defined twice with different periods, and the primary clock uses a
// made-up source pin. Lint tools and the PnR flow will either pick one
// arbitrarily or error out. One clock per source, defined on the actual
// clock port.
create_clock -name clk -period 10.000 -waveform {0 5} [get_ports clk]
create_clock -name clk_bogus -period 7.500 [get_ports clk]

// Wrong: clock defined on a data pin.
create_clock -name clk_data -period 5.000 [get_ports data_in]

// Wrong: undefined object — no such port exists, SDC parser will warn.
set_input_delay 2.0 -clock clk [get_ports nonexistent_pin]
