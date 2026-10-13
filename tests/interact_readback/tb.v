`timescale 1ns/1ps
module tb;
  reg clk_74a = 0; always #6.734 clk_74a = ~clk_74a;
  reg [31:0] bridge_addr = 0, bridge_wr_data = 0;
  reg bridge_rd = 0, bridge_wr = 0;
  reg [31:0] bridge_rd_data;
  reg [31:0] cmd_bridge_rd_data = 32'hDEAD_BEEF, ss_bridge_rd_q = 32'h5555_5555, dataslot_table_rd_data = 32'h1234_5678;
  reg [7:0] osd_offset_x, osd_offset_y; reg [1:0] control_mode; reg sid_model, sid_dual, warp_mode; reg [1:0] drive_snd_level;
`include "ia_block.vh"
`include "mux.vh"
  reg [31:0] model[0:6]; reg [31:0] rdv; integer i, f, err = 0, prev;
  // bridge read: data sampled 3 cycles after the address, then bridge_rd; the sample belongs to the previous read
  task bread(input [31:0] a, output [31:0] v);
    begin
      @(posedge clk_74a) bridge_addr <= a;
      repeat (3) @(posedge clk_74a); v = bridge_rd_data;
      @(posedge clk_74a) bridge_rd <= 1; @(posedge clk_74a) bridge_rd <= 0;
      repeat (24) @(posedge clk_74a);
    end
  endtask
  task bwrite(input [31:0] a, input [31:0] d);
    begin
      @(posedge clk_74a) begin bridge_addr <= a; bridge_wr_data <= d; end
      repeat (3) @(posedge clk_74a); @(posedge clk_74a) bridge_wr <= 1; @(posedge clk_74a) bridge_wr <= 0;
      repeat (24) @(posedge clk_74a);
    end
  endtask
  task check_fields;
    begin
      if (osd_offset_x !== model[0][7:0] || osd_offset_y !== model[1][7:0] || control_mode !== model[2][1:0] ||
          sid_model !== model[3][0] || sid_dual !== model[4][0] || warp_mode !== model[5][0] || drive_snd_level !== model[6][1:0]) begin
        $display("FAIL fields"); err = err + 1; end
    end
  endtask
  initial begin
    model[0]=1; model[1]=1; model[2]=0; model[3]=0; model[4]=0; model[5]=1; model[6]=2;
    // defaults read back (read-ahead pipeline: read of i+1 returns i)
    bread(32'hA000_0000, rdv);
    for (i = 1; i <= 7; i = i + 1) begin
      bread(32'hA000_0000 + (i % 7) * 4, rdv);
      if (rdv !== model[i-1]) begin $display("FAIL default %0d: %h != %h", i-1, rdv, model[i-1]); err = err + 1; end
    end
    // boot writes of persisted values, odd upper bits too
    for (i = 0; i < 7; i = i + 1) begin model[i] = $random; bwrite(32'hA000_0000 + i*4, model[i]); end
    check_fields;
    // frames of RMW in fixed order: read all (one-late), write all back, sometimes with a user change
    for (f = 0; f < 50; f = f + 1) begin
      bread(32'hA000_0000, rdv);
      for (i = 1; i <= 7; i = i + 1) begin
        bread(32'hA000_0000 + (i % 7) * 4, rdv);
        if (rdv !== model[i-1]) begin $display("FAIL f%0d w%0d: %h != %h", f, i-1, rdv, model[i-1]); err = err + 1; end
      end
      if (f % 5 == 0) model[f % 7] = $random % 4;
      for (i = 0; i < 7; i = i + 1) bwrite(32'hA000_0000 + i*4, model[i]);
      // interleaved read X, read X, write X
      for (i = 0; i < 7; i = i + 1) begin
        bread(32'hA000_0000 + i*4, rdv); bread(32'hA000_0000 + i*4, rdv);
        if (rdv !== model[i]) begin $display("FAIL rrw f%0d w%0d", f, i); err = err + 1; end
        bwrite(32'hA000_0000 + i*4, model[i]);
      end
      check_fields;
    end
    // other regions untouched
    bread(32'hF800_2000, rdv); bread(32'hF800_2004, rdv); if (rdv !== 32'h1234_5678) begin $display("FAIL dataslot"); err = err + 1; end
    bread(32'hB000_0000, rdv); if (rdv !== 32'h5555_5555) begin $display("FAIL ss"); err = err + 1; end
    bread(32'hF800_0000, rdv); if (rdv !== 32'hDEAD_BEEF) begin $display("FAIL cmd"); err = err + 1; end
    if (err == 0) $display("PASS"); else $display("FAILED %0d", err);
    $finish;
  end
endmodule
