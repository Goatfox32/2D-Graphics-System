module rasterizer #(
	parameter int FB_WIDTH = 320,
	parameter int FB_HEIGHT = 240,
	parameter int PIXEL_SIZE = 6,
	parameter int X_WIDTH = 9,
	parameter int Y_WIDTH = 8
)
(
	input logic clk,
	input logic s1,
	
	input logic [191:0] vertex_data,

	output logic rast_ready,

	output logic write_en,
	output logic [X_WIDTH-1:0] write_x,
	output logic [Y_WIDTH-1:0] write_y,
	output logic [PIXEL_SIZE-1:0] write_color
);
	////////// max & min function /////////
	function automatic [X_WIDTH-1:0] max3x
		( 
		input [X_WIDTH-1:0] a,
		input [X_WIDTH-1:0] b,
		input [X_WIDTH-1:0] c
		);
	
		begin
			max3x = (a >= b) ? ((a >= c) ? a : c)
							    : ((b >= c) ? b : c);
		end
	endfunction
	
	function automatic [X_WIDTH-1:0] min3x
		( 
		input [X_WIDTH-1:0] a,
		input [X_WIDTH-1:0] b,
		input [X_WIDTH-1:0] c
		);
	
		begin
			min3x = (a <= b) ? ((a <= c) ? a : c)
							    : ((b <= c) ? b : c);
		end
	endfunction
	
	function automatic [Y_WIDTH-1:0] max3y
		( 
		input [Y_WIDTH-1:0] a,
		input [Y_WIDTH-1:0] b,
		input [Y_WIDTH-1:0] c
		);
	
		begin
			max3y = (a >= b) ? ((a >= c) ? a : c)
							    : ((b >= c) ? b : c);
		end
	endfunction
	
	function automatic [Y_WIDTH-1:0] min3y
		( 
		input [Y_WIDTH-1:0] a,
		input [Y_WIDTH-1:0] b,
		input [Y_WIDTH-1:0] c
		);
	
		begin
			min3y = (a <= b) ? ((a <= c) ? a : c)
							    : ((b <= c) ? b : c);
		end
	endfunction
	
   ////////////// Components /////////////
	
	// 16 bits color 
	
	logic [63:0] v1, v2, v3;

	assign v1 = vertex_data[191:128];
	assign v2 = vertex_data[127:64];
	assign v3 = vertex_data[63:0];
	
	
	logic [X_WIDTH-1:0] x1, x2, x3;
	
	assign x1 = v1[8:0];
	assign x2 = v2[8:0];
	assign x3 = v3[8:0];
	
	logic [Y_WIDTH-1:0] y1, y2, y3;
	
	assign y1 = v1[16:9];
	assign y2 = v2[16:9];
	assign y3 = v3[16:9];
	
	logic [15:0] color1, color2, color3;
	
	assign color1 = v1[32:17];
	assign color2 = v2[32:17];
	assign color3 = v3[32:17];
	
	// Intermediate Calculating Variables
	
	logic signed [31:0] sx1, sx2, sx3;
	logic signed [31:0] sy1, sy2, sy3;
	logic signed [31:0] sx_curr, sy_curr;

	assign sx1 = $signed({1'b0, x1});
	assign sx2 = $signed({1'b0, x2});
	assign sx3 = $signed({1'b0, x3});
	assign sy1 = $signed({1'b0, y1});
	assign sy2 = $signed({1'b0, y2});
	assign sy3 = $signed({1'b0, y3});
	assign sx_curr = $signed({1'b0, x_curr});
	assign sy_curr = $signed({1'b0, y_curr}); 
	
	logic [4:0] r1, r2, r3, r_mix;
	logic [5:0] g1, g2, g3, g_mix;
	logic [4:0] b1, b2, b3, b_mix;
	
	assign r1 = color1[4:0];
	assign g1 = color1[10:5];
	assign b1 = color1[15:11];

	assign r2 = color1[4:0];
	assign g2 = color1[10:5];
	assign b2 = color1[15:11];

	assign r3 = color1[4:0];
	assign g3 = color1[10:5];
	assign b3 = color1[15:11];

	
	logic pixel_valid;
	logic signed [31:0] area, area_n;
	logic signed [31:0] e1_n, e2_n, e3_n;
	logic signed [47:0] r_num, g_num, b_num;
	logic signed [31:0] e1, e2, e3;
	logic [PIXEL_SIZE-1:0] mixed_color;
	
	
   ////////////// Bounding box //////////////
	
	logic [X_WIDTH-1:0] x_max, x_min, x_curr;
	
	assign x_max = max3x(x1,x2,x3);
	assign x_min = min3x(x1,x2,x3);
	
	logic [Y_WIDTH-1:0] y_max, y_min, y_curr;
	
	assign y_max = max3y(y1,y2,y3);
	assign y_min = min3y(y1,y2,y3);
	
	always_comb begin
	
		e1 = '0;
		e2 = '0;
		e3 = '0;
		area = '0;

		area_n = '0;
		e1_n = '0;
		e2_n = '0;
		e3_n = '0;

		r_num = '0;
		g_num = '0;
		b_num = '0;

		r_mix = '0;
		g_mix = '0;
		b_mix = '0;
		mixed_color = '0;
		pixel_valid = 1'b0;

		area = sx1*(sy2 - sy3) + sx2*(sy3 - sy1) + sx3*(sy1 - sy2);
		
		// Edge equations and windings
		if (area != 0) begin
			e1 = sx_curr*(sy1 - sy2) + sy_curr*(sx2 - sx1) + (sx1*sy2 - sx2*sy1);
			e2 = sx_curr*(sy2 - sy3) + sy_curr*(sx3 - sx2) + (sx2*sy3 - sx3*sy2);
			e3 = sx_curr*(sy3 - sy1) + sy_curr*(sx1 - sx3) + (sx3*sy1 - sx1*sy3);
			
			if (area < 0) begin // if negative winding -> negate all negatives
				area_n = -area;
				e1_n   = -e1;
				e2_n   = -e2;
				e3_n   = -e3;
			end else begin // if positive winding -> leave unchanged
				area_n = area;
				e1_n = e1;
				e2_n = e2;
				e3_n = e3;
			end
			
			if ((e1_n >= 0) && (e2_n >= 0) && (e3_n >= 0)) // if inside the triangle -> pixel is valid
				pixel_valid = 1;
			
			// Color mixing calculation
			if (pixel_valid) begin
				r_num = e2_n * $signed({1'b0, r1}) +
						  e3_n * $signed({1'b0, r2}) +
						  e1_n * $signed({1'b0, r3});

				g_num = e2_n * $signed({1'b0, g1}) +
						  e3_n * $signed({1'b0, g2}) +
						  e1_n * $signed({1'b0, g3});

				b_num = e2_n * $signed({1'b0, b1}) +
						  e3_n * $signed({1'b0, b2}) +
						  e1_n * $signed({1'b0, b3});

				r_mix = r_num / area_n;
				g_mix = g_num / area_n;
				b_mix = b_num / area_n;

				mixed_color = {r_mix[4:3], g_mix[5:4], b_mix[4:3]};
			end
		end
	end

	always_ff @(posedge clk) begin
	
		if (~s1) begin
			  x_curr <= x_min;
			  y_curr <= y_min;
			  rast_ready <= '0;
		end else begin
		
			if (y_curr <= y_max) begin
			
				rast_ready <= '0;
				
				if (x_curr < x_max) begin
					x_curr <= x_curr + 1;
				end else begin
					x_curr <= x_min;
					y_curr <= y_curr + 1;
				end
				
			end else begin
				rast_ready <= 1;
			end
				
		end
	end
		
		assign write_en = pixel_valid;
		assign write_x = pixel_valid ? (x_curr) : '0;
		assign write_y = pixel_valid ? (y_curr) : '0;
		assign write_color = pixel_valid ? mixed_color : '0;
endmodule