//
// 1541 drive sounds, played from samples like the 1541 Ultimate / Ultimate 64
// (floppy_sound.vhd) and using the same sample file layout (snds1541.bin,
// 8-bit signed, 22050 Hz):
//
//   0x0000  motor hum without a disk, 4410 samples looped
//   0x1200  motor with a disk (slip), 4410 samples looped
//   0x2400  head step inwards, 0x800 samples
//   0x2C00  head step outwards, 0x880 samples
//   0x3480  head bang against the track 1 stop, 0x880 samples
//
// The file is loaded by the BIOS through the write port; without it the RAM
// stays zero and everything is silent. Steps restart the one-shot voice, but
// not within 3 ms of the last start, so warp gives a rattle instead of a buzz.
// Runs on real time (no system clock enable).
//
module drive_sound (
    input clk,  // 32 MHz
    input motor_on,
    input disk_in,
    input [1:0] head_phase,  // stepper phase, a change is one half track step
    input head_at_stop,  // half track 0: a step outwards bangs the stop
    input [1:0] level,  // 0: Off, 1: Low, 2: Medium, 3: High
    output reg signed [15:0] sample = 0,

    input wr_clk,
    input wr_en,
    input [13:0] wr_addr,
    input [7:0] wr_data
);

  reg [7:0] mem[0:16383];
  reg [13:0] rd_addr = 0;
  reg [7:0] rd_data = 0;
  always @(posedge wr_clk) if (wr_en) mem[wr_addr] <= wr_data;
  always @(posedge clk) rd_data <= mem[rd_addr];

  // 22050 Hz sample tick
  reg [10:0] div = 0;
  wire tick = (div == 11'd1450);
  always @(posedge clk) div <= tick ? 11'd0 : div + 1'b1;

  // Step events are held until the next tick
  localparam [1:0] EV_NONE = 0, EV_IN = 1, EV_OUT = 2, EV_BANG = 3;
  reg [1:0] phase_p = 0;
  reg [1:0] ev = EV_NONE;
  always @(posedge clk) begin
    phase_p <= head_phase;
    if (tick) ev <= EV_NONE;
    if (head_phase == phase_p + 2'd1) ev <= EV_IN;
    else if (head_phase == phase_p - 2'd1) ev <= head_at_stop ? EV_BANG : EV_OUT;
  end

  // Voice 1: one-shot head sounds
  reg [13:0] v1_addr = 0;
  reg [11:0] v1_cnt = 0;  // samples left
  reg [11:0] v1_age = 0;  // samples since start
  // Voice 2: looped motor
  reg [12:0] v2_pos = 0;

  reg signed [7:0] v1_smp = 0, v2_smp = 0;
  reg [3:0] seq = 0;
  always @(posedge clk) begin
    seq <= {seq[2:0], tick};

    if (tick) begin
      if (ev != EV_NONE && (v1_cnt == 0 || v1_age >= 12'd66)) begin
        v1_age <= 0;
        case (ev)
          EV_IN: begin v1_addr <= 14'h2400; v1_cnt <= 12'h800; end
          EV_OUT: begin v1_addr <= 14'h2C00; v1_cnt <= 12'h880; end
          default: begin v1_addr <= 14'h3480; v1_cnt <= 12'h880; end
        endcase
      end else if (v1_age != 12'hfff) v1_age <= v1_age + 1'b1;
    end

    // Read voice 1, then voice 2 (one cycle RAM latency)
    if (seq[0]) rd_addr <= v1_addr;
    if (seq[1]) rd_addr <= (disk_in ? 14'h1200 : 14'h0000) + v2_pos;
    if (seq[2]) begin
      v1_smp <= (v1_cnt != 0) ? rd_data : 8'd0;
      if (v1_cnt != 0) begin
        v1_cnt <= v1_cnt - 1'b1;
        v1_addr <= v1_addr + 1'b1;
      end
    end
    if (seq[3]) begin
      v2_smp <= motor_on ? rd_data : 8'd0;
      v2_pos <= (!motor_on || v2_pos == 13'd4409) ? 13'd0 : v2_pos + 1'b1;
    end
  end

  // Mix like the Ultimate (head sounds 16x the motor), then the level
  wire signed [12:0] mix = v2_smp + (v1_smp <<< 4);
  wire signed [17:0] scaled = mix <<< ({1'b0, level} + 3'd1);
  always @(posedge clk)
    if (seq[3])
      sample <= (level == 0) ? 16'sd0 :
                (scaled > 18'sd32767) ? 16'sd32767 :
                (scaled < -18'sd32768) ? -16'sd32768 : scaled[15:0];

endmodule
