/*
 * Modified VGA Counters: 1024x768 @ 60Hz @ 65MHz
 *
 * Chess Group: Hooman Khaloo, Hongchi Liu, and Pengfei Yan
 * Spring 2025
 * Columbia University
 */

module vga_counters (
    input  logic        clk,
    input  logic        reset,
    output logic [10:0] hcount,
    output logic [9:0]  vcount,
    output logic        VGA_HS,
    output logic        VGA_VS,
    output logic        VGA_BLANK_n,
    output logic        VGA_SYNC_n,
    output logic        VGA_CLK);

    parameter HACTIVE      = 11'd1024;
    parameter HFRONT_PORCH = 11'd24;
    parameter HSYNC        = 11'd136;
    parameter HBACK_PORCH  = 11'd160;
    parameter HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH;

    parameter VACTIVE      = 10'd768;
    parameter VFRONT_PORCH = 10'd3;
    parameter VSYNC        = 10'd6;
    parameter VBACK_PORCH  = 10'd29;
    parameter VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC + VBACK_PORCH;

    logic endOfLine, endOfField;

    always_ff @(posedge clk or posedge reset)
        if (reset)
            hcount <= 0;
        else if (endOfLine)
            hcount <= 0;
        else
            hcount <= hcount + 1;

    assign endOfLine = (hcount == HTOTAL - 1);

    always_ff @(posedge clk or posedge reset)
        if (reset)
            vcount <= 0;
        else if (endOfLine)
            if (endOfField)
                vcount <= 0;
            else
                vcount <= vcount + 1;

    assign endOfField = (vcount == VTOTAL - 1);

    assign VGA_HS = !(
        hcount >= (HACTIVE + HFRONT_PORCH) &&
        hcount <  (HACTIVE + HFRONT_PORCH + HSYNC)
    );

    assign VGA_VS = !(
        vcount >= (VACTIVE + VFRONT_PORCH) &&
        vcount <  (VACTIVE + VFRONT_PORCH + VSYNC)
    );

    assign VGA_BLANK_n = (hcount < HACTIVE) && (vcount < VACTIVE);
    assign VGA_SYNC_n = 1'b0;
    assign VGA_CLK = clk;
endmodule
