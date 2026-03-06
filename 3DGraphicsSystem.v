// Date: March 3rd, 2026
// Name: Jacob Edwards
// Student #: A01360840
module 3DGraphicsSystem 
(


)

endmodule

module triangle_rastenizer #(
	parameter int H_RES = 320, // Horizontal Resolution
	parameter int V_RES	= 240, // Vertical Resolution
	parameter int COL_W = 16,   // Color width (RGB 565)
	parameter int EW = 24,    // Edge width, made very big so that triangles dont overflow
	parameter int ADD_W = 17, // Address Width for a frame buffer large enough to hold H_RES*V_RES (log2(76800)
	parameter int VTEX = 10
	)(
	input logic clk,
	input logic reset,
	
	logic [EW-1:0] edge1,
	logic [EW-1:0] edge2,
	logic [EW-1:0] edge3,

	logic [VTEX-1:0] data,
	
	
	);
	
	// body
	
	endmodule