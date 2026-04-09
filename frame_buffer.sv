module frame_buffer #(
   parameter H_SYNC_MAX = 800, // 640 visble pixel timings, 16 front porch timings, 96 H-Sync timings, and 48 back porch timings
   parameter V_SYNC_MAX = 525, // 480 visble line timings, 10 front porch timingss, 2 V-Sync timings, and 33 back porch timings
   parameter int FB_WIDTH  = 320, // Frame buffer width
   parameter int FB_HEIGHT = 240, // Frame buffer height
   parameter int PIXEL_SIZE = 6, //Bits per pixel
   parameter int SIZE = FB_WIDTH * FB_HEIGHT,
   parameter int ADDR_W = $clog2(SIZE), // Address width
   parameter int X_WIDTH = $clog2(FB_WIDTH), // Bits per X-coord
   parameter int Y_WIDTH = $clog2(FB_HEIGHT) // Bits per Y-coord
)(
	input logic clk,
	input logic write_en, // flag to allow data to be written
	input logic [X_WIDTH-1:0] write_x, // x position of pixel
	input logic [Y_WIDTH-1:0] write_y, // y position of pixel
	input logic [PIXEL_SIZE-1:0] write_data, // data being written into frame buffer from rasterzier
	input logic [X_WIDTH-1:0] read_x,
	input logic [Y_WIDTH-1:0] read_y,
	output logic [PIXEL_SIZE-1:0] read_data // store pixel data
);

	logic [PIXEL_SIZE-1:0] mem [0:SIZE-1]; // Memory array that stores pixel data
   logic [ADDR_W-1:0] write_addr, read_addr; // Write Address, and Read Address
	logic visible_pixel;
	
	always_comb begin
    read_addr = FB_WIDTH*read_y + read_x;
    write_addr = FB_WIDTH*write_y + write_x;
	end
		
	
	always_ff @(posedge clk) begin
		
		read_data <= mem[read_addr]; // pull pixel data from memory address and store it

		if (write_en && (write_x < FB_WIDTH) && (write_y < FB_HEIGHT))
			mem[write_addr] <= write_data; // write data to address
		
	end
		
	
endmodule