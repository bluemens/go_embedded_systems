/*
 * Theme memory to store colors
 *
 * Chess Group: Hooman Khaloo, Hongchi Liu, and Pengfei Yan
 * Spring 2025
 * Columbia University
 */

module theme_mem (
    input  logic [3:0] color_id,
    output logic [119:0] color_out
);
    
    always_comb begin
        case (color_id)
            4'd0: color_out = {
                /* Blue theme with Black background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'hFF, 8'hFF, 8'hFF,    // White piece
                8'h29, 8'h64, 8'h95,    // Black box
                8'ha4, 8'hcc, 8'hfb,    // White box
                8'h00, 8'h00, 8'h00     // Background
            }; 4'd1: color_out = {
                /* Blue theme with White background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'hFF, 8'hFF, 8'hFF,    // White piece
                8'h29, 8'h64, 8'h95,    // Black box
                8'ha4, 8'hcc, 8'hfb,    // White box
                8'hff, 8'hff, 8'hff     // Background
            }; 4'd2: color_out = {
                /* Blue theme with Blue background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'hFF, 8'hFF, 8'hFF,    // White piece
                8'h29, 8'h64, 8'h95,    // Black box
                8'ha4, 8'hcc, 8'hfb,    // White box
                8'h00, 8'h00, 8'hff     // Background
            }; 4'd3: color_out = {
                /* Blue theme with Green background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'hFF, 8'hFF, 8'hFF,    // White piece
                8'h29, 8'h64, 8'h95,    // Black box
                8'ha4, 8'hcc, 8'hfb,    // White box
                8'h00, 8'hff, 8'h00     // Background
            }; 4'd4: color_out = {
                /* Red theme with Black background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'h88, 8'h86, 8'h0b,    // White piece
                8'hb0, 8'h61, 8'h41,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'h00, 8'h00, 8'h00     // Background
            }; 4'd5: color_out = {
                /* Red theme with white background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'h88, 8'h86, 8'h0b,    // White piece
                8'hb0, 8'h61, 8'h41,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'hff, 8'hff, 8'hff     // Background
            }; 4'd6: color_out = {
                /* Red theme with Blue background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'h88, 8'h86, 8'h0b,    // White piece
                8'hb0, 8'h61, 8'h41,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'h00, 8'h00, 8'hff     // Background
            }; 4'd7: color_out = {
                /* Red theme with Green background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'h88, 8'h86, 8'h0b,    // White piece
                8'hb0, 8'h61, 8'h41,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'h00, 8'hff, 8'h00     // Background
            }; 4'd8: color_out = {
                /* Greeen theme with Black background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'hd9, 8'hd7, 8'hbc,    // White piece
                8'h82, 8'h93, 8'h63,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'h00, 8'h00, 8'h00     // Background
            }; 4'd9: color_out = {
                /* Green theme with White background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'hd9, 8'hd7, 8'hbc,    // White piece
                8'h82, 8'h93, 8'h63,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'hff, 8'hff, 8'hff     // Background
            }; 4'd10: color_out = {
                /* Green theme with Blue background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'hd9, 8'hd7, 8'hbc,    // White piece
                8'h82, 8'h93, 8'h63,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'h00, 8'h00, 8'hff     // Background
            }; 4'd11: color_out = {
                /* Green theme with Green background */
                8'h00, 8'h00, 8'h00,    // Black piece
                8'hd9, 8'hd7, 8'hbc,    // White piece
                8'h82, 8'h93, 8'h63,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'h00, 8'hff, 8'h00     // Background
            }; 4'd12: color_out = {
                /* Classical theme with Black background */
                8'h29, 8'h29, 8'h2e,    // Black piece
                8'hd9, 8'hd7, 8'hbc,    // White piece
                8'h94, 8'h8e, 8'h94,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'h00, 8'h00, 8'h00     // Background
            }; 4'd13: color_out = {
                /* Classical theme with White background */
                8'h29, 8'h29, 8'h2e,    // Black piece
                8'hd9, 8'hd7, 8'hbc,    // White piece
                8'h94, 8'h8e, 8'h94,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'hff, 8'hff, 8'hff     // Background
            }; 4'd14: color_out = {
                /* Classical theme with Blue background */
                8'h29, 8'h29, 8'h2e,    // Black piece
                8'hd9, 8'hd7, 8'hbc,    // White piece
                8'h94, 8'h8e, 8'h94,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'h00, 8'h00, 8'hff     // Background
            }; 4'd15: color_out = {
                /* Classical theme with Green background */
                8'h29, 8'h29, 8'h2e,    // Black piece
                8'hd9, 8'hd7, 8'hbc,    // White piece
                8'h94, 8'h8e, 8'h94,    // Black box
                8'heb, 8'heb, 8'heb,    // White box
                8'h00, 8'hff, 8'h00     // Background
            };

            default: color_out = {
                8'h00, 8'h00, 8'h00,    // Black piece: RGB
                8'hFF, 8'hFF, 8'hFF,    // White piece: RGB
                8'h29, 8'h64, 8'h95,    // Black box: RGB
                8'ha4, 8'hcc, 8'hfb,    // White box: RGB
                8'h00, 8'h00, 8'h00     // Background: RGB
            };
        endcase
    end

endmodule
