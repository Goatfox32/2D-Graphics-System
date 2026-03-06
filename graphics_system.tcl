package require -exact qsys 14.0

create_system {graphics_system}
set_project_property DEVICE_FAMILY {Cyclone V}
set_project_property DEVICE {5CSEMA4U23C6}

# ==========================================
# 1. Add Components
# ==========================================
add_instance clk_0 clock_source
set_instance_parameter_value clk_0 {clockFrequency} {50000000}
set_instance_parameter_value clk_0 {clockFrequencyKnown} {true}

add_instance hps_0 altera_hps

# Enable Peripherals and Route to Physical Pins
set_instance_parameter_value hps_0 {SDIO_PinMuxing} {HPS I/O Set 0}
set_instance_parameter_value hps_0 {SDIO_Mode} {4-bit Data}
set_instance_parameter_value hps_0 {UART0_PinMuxing} {HPS I/O Set 0}
set_instance_parameter_value hps_0 {UART0_Mode} {No Flow Control}
set_instance_parameter_value hps_0 {USB1_PinMuxing} {HPS I/O Set 0}
set_instance_parameter_value hps_0 {USB1_Mode} {SDR}
set_instance_parameter_value hps_0 {EMAC1_PinMuxing} {HPS I/O Set 0}
set_instance_parameter_value hps_0 {EMAC1_Mode} {RGMII}

add_instance pio altera_avalon_pio
set_instance_parameter_value pio {direction} {Output}
set_instance_parameter_value pio {width} {8}

# ==========================================
# 2. Internal Connections
# ==========================================
# Clocks
add_connection clk_0.clk hps_0.f2h_axi_clock
add_connection clk_0.clk hps_0.f2h_sdram0_clock
add_connection clk_0.clk hps_0.h2f_axi_clock
add_connection clk_0.clk hps_0.h2f_lw_axi_clock
add_connection clk_0.clk pio.clk

# Resets
add_connection hps_0.h2f_reset clk_0.clk_in_reset
add_connection clk_0.clk_reset pio.reset

# Data Bridge (HPS to PIO)
add_connection hps_0.h2f_lw_axi_master pio.s1
set_connection_parameter_value hps_0.h2f_lw_axi_master/pio.s1 baseAddress {0x0000}

# ==========================================
# 3. External Exports (To top-level .sv file)
# ==========================================
add_interface clk50 clock sink
set_interface_property clk50 EXPORT_OF clk_0.clk_in

add_interface clk_reset reset sink
set_interface_property clk_reset EXPORT_OF clk_0.clk_in_reset

add_interface memory conduit end
set_interface_property memory EXPORT_OF hps_0.memory

add_interface hps_io conduit end
set_interface_property hps_io EXPORT_OF hps_0.hps_io

add_interface pio_export conduit end
set_interface_property pio_export EXPORT_OF pio.external_connection

# Save it!
save_system {graphics_system.qsys}