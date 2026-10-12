//
// User core top-level
//
// Instantiated by the real top-level: apf_top
//

`default_nettype none

module core_top (

    //
    // physical connections
    //

    ///////////////////////////////////////////////////
    // clock inputs 74.25mhz. not phase aligned, so treat these domains as asynchronous

    input wire clk_74a,  // mainclk1
    input wire clk_74b,  // mainclk1
`ifdef __VERILATOR__
    input wire reset_n,
    input wire clk_32mhz,
`endif
    output wire debug_iec_atn,
    output wire debug_iec_data,
    output wire debug_iec_clock,
    output wire debug_1mhz_ph1_en,
    output wire debug_1mhz_ph2_en,

    output wire debug_c64_cpu_valid,
    output wire debug_c64_cpu_sync,
    output wire [15:0] debug_c64_cpu_addr,
    output wire [7:0] debug_c64_cpu_data,
    output wire [63:0]debug_c64_cpu_regs,

    output wire debug_c1541_cpu_valid,
    output wire debug_c1541_cpu_sync,
    output wire [15:0] debug_c1541_cpu_addr,
    output wire [7:0] debug_c1541_cpu_data,
    output wire [63:0]debug_c1541_cpu_regs,

    ///////////////////////////////////////////////////
    // cartridge interface
    // switches between 3.3v and 5v mechanically
    // output enable for multibit translators controlled by pic32

    // GBA AD[15:8]
    inout  wire [7:0] cart_tran_bank2,
    output wire       cart_tran_bank2_dir,

    // GBA AD[7:0]
    inout  wire [7:0] cart_tran_bank3,
    output wire       cart_tran_bank3_dir,

    // GBA A[23:16]
    inout  wire [7:0] cart_tran_bank1,
    output wire       cart_tran_bank1_dir,

    // GBA [7] PHI#
    // GBA [6] WR#
    // GBA [5] RD#
    // GBA [4] CS1#/CS#
    //     [3:0] unwired
    inout  wire [7:4] cart_tran_bank0,
    output wire       cart_tran_bank0_dir,

    // GBA CS2#/RES#
    inout  wire cart_tran_pin30,
    output wire cart_tran_pin30_dir,
    // when GBC cart is inserted, this signal when low or weak will pull GBC /RES low with a special circuit
    // the goal is that when unconfigured, the FPGA weak pullups won't interfere.
    // thus, if GBC cart is inserted, FPGA must drive this high in order to let the level translators
    // and general IO drive this pin.
    output wire cart_pin30_pwroff_reset,

    // GBA IRQ/DRQ
    inout  wire cart_tran_pin31,
    output wire cart_tran_pin31_dir,

    // infrared
    input  wire port_ir_rx,
    output wire port_ir_tx,
    output wire port_ir_rx_disable,

    // GBA link port
    inout  wire port_tran_si,
    output wire port_tran_si_dir,
    inout  wire port_tran_so,
    output wire port_tran_so_dir,
    inout  wire port_tran_sck,
    output wire port_tran_sck_dir,
    inout  wire port_tran_sd,
    output wire port_tran_sd_dir,

    ///////////////////////////////////////////////////
    // cellular psram 0 and 1, two chips (64mbit x2 dual die per chip)

    output wire [21:16] cram0_a,
    inout  wire [ 15:0] cram0_dq,
    input  wire         cram0_wait,
    output wire         cram0_clk,
    output wire         cram0_adv_n,
    output wire         cram0_cre,
    output wire         cram0_ce0_n,
    output wire         cram0_ce1_n,
    output wire         cram0_oe_n,
    output wire         cram0_we_n,
    output wire         cram0_ub_n,
    output wire         cram0_lb_n,

    output wire [21:16] cram1_a,
    inout  wire [ 15:0] cram1_dq,
    input  wire         cram1_wait,
    output wire         cram1_clk,
    output wire         cram1_adv_n,
    output wire         cram1_cre,
    output wire         cram1_ce0_n,
    output wire         cram1_ce1_n,
    output wire         cram1_oe_n,
    output wire         cram1_we_n,
    output wire         cram1_ub_n,
    output wire         cram1_lb_n,

    ///////////////////////////////////////////////////
    // sdram, 512mbit 16bit

    output wire [12:0] dram_a,
    output wire [ 1:0] dram_ba,
    inout  wire [15:0] dram_dq,
    output wire [ 1:0] dram_dqm,
    output wire        dram_clk,
    output wire        dram_cke,
    output wire        dram_ras_n,
    output wire        dram_cas_n,
    output wire        dram_we_n,

    ///////////////////////////////////////////////////
    // sram, 1mbit 16bit

    output wire [16:0] sram_a,
    inout  wire [15:0] sram_dq,
    output wire        sram_oe_n,
    output wire        sram_we_n,
    output wire        sram_ub_n,
    output wire        sram_lb_n,

    ///////////////////////////////////////////////////
    // vblank driven by dock for sync in a certain mode

    input wire vblank,

    ///////////////////////////////////////////////////
    // i/o to 6515D breakout usb uart

    output wire dbg_tx,
    input  wire dbg_rx,

    ///////////////////////////////////////////////////
    // i/o pads near jtag connector user can solder to

    output wire user1,
    input  wire user2,

    ///////////////////////////////////////////////////
    // RFU internal i2c bus

    inout  wire aux_sda,
    output wire aux_scl,

    ///////////////////////////////////////////////////
    // RFU, do not use
    output wire vpll_feed,


    //
    // logical connections
    //

    ///////////////////////////////////////////////////
    // video, audio output to scaler
    output wire [23:0] video_rgb,
    output wire        video_rgb_clock,
    output wire        video_rgb_clock_90,
    output wire        video_de,
    output wire        video_skip,
    output wire        video_vs,
    output wire        video_hs,

    output wire audio_mclk,
    input  wire audio_adc,
    output wire audio_dac,
    output wire audio_lrck,

    ///////////////////////////////////////////////////
    // bridge bus connection
    // synchronous to clk_74a
    output wire        bridge_endian_little,
    input  wire [31:0] bridge_addr,
    input  wire        bridge_rd,
    output reg  [31:0] bridge_rd_data,
    input  wire        bridge_wr,
    input  wire [31:0] bridge_wr_data,

    ///////////////////////////////////////////////////
    // controller data
    //
    // key bitmap:
    //   [0]    dpad_up
    //   [1]    dpad_down
    //   [2]    dpad_left
    //   [3]    dpad_right
    //   [4]    face_a
    //   [5]    face_b
    //   [6]    face_x
    //   [7]    face_y
    //   [8]    trig_l1
    //   [9]    trig_r1
    //   [10]   trig_l2
    //   [11]   trig_r2
    //   [12]   trig_l3
    //   [13]   trig_r3
    //   [14]   face_select
    //   [15]   face_start
    // joy values - unsigned
    //   [ 7: 0] lstick_x
    //   [15: 8] lstick_y
    //   [23:16] rstick_x
    //   [31:24] rstick_y
    // trigger values - unsigned
    //   [ 7: 0] ltrig
    //   [15: 8] rtrig
    //
    input wire [15:0] cont1_key,
    input wire [15:0] cont2_key,
    input wire [15:0] cont3_key,
    input wire [15:0] cont4_key,
    input wire [31:0] cont1_joy,
    input wire [31:0] cont2_joy,
    input wire [31:0] cont3_joy,
    input wire [31:0] cont4_joy,
    input wire [15:0] cont1_trig,
    input wire [15:0] cont2_trig,
    input wire [15:0] cont3_trig,
    input wire [15:0] cont4_trig

);

  // not using the IR port, so turn off both the LED, and
  // disable the receive circuit to save power
  assign port_ir_tx              = 0;
  assign port_ir_rx_disable      = 1;

  // bridge endianness
  assign bridge_endian_little    = 0;

  // cart is unused, so set all level translators accordingly
  // directions are 0:IN, 1:OUT
  assign cart_tran_bank3         = 8'hzz;
  assign cart_tran_bank3_dir     = 1'b0;
  assign cart_tran_bank2         = 8'hzz;
  assign cart_tran_bank2_dir     = 1'b0;
  assign cart_tran_bank1         = 8'hzz;
  assign cart_tran_bank1_dir     = 1'b0;
  assign cart_tran_bank0         = 4'hf;
  assign cart_tran_bank0_dir     = 1'b1;
  assign cart_tran_pin30         = 1'b0;  // reset or cs2, we let the hw control it by itself
  assign cart_tran_pin30_dir     = 1'bz;
  assign cart_pin30_pwroff_reset = 1'b0;  // hardware can control this
  assign cart_tran_pin31         = 1'bz;  // input
  assign cart_tran_pin31_dir     = 1'b0;  // input

  // link port is input only
  assign port_tran_so            = 1'bz;
  assign port_tran_so_dir        = 1'b0;  // SO is output only
  assign port_tran_si            = 1'bz;
  assign port_tran_si_dir        = 1'b0;  // SI is input only
  assign port_tran_sck           = 1'bz;
  assign port_tran_sck_dir       = 1'b0;  // clock direction can change
  assign port_tran_sd            = 1'bz;
  assign port_tran_sd_dir        = 1'b0;  // SD is input and not used

  // tie off the rest of the pins we are not using
  assign cram1_a                 = 'h0;
  assign cram1_dq                = {16{1'bZ}};
  assign cram1_clk               = 0;
  assign cram1_adv_n             = 1;
  assign cram1_cre               = 0;
  assign cram1_ce0_n             = 1;
  assign cram1_ce1_n             = 1;
  assign cram1_oe_n              = 1;
  assign cram1_we_n              = 1;
  assign cram1_ub_n              = 1;
  assign cram1_lb_n              = 1;

  assign dram_a                  = 'h0;
  assign dram_ba                 = 'h0;
  assign dram_dq                 = {16{1'bZ}};
  assign dram_dqm                = 'h0;
  assign dram_clk                = 'h0;
  assign dram_cke                = 'h0;
  assign dram_ras_n              = 'h1;
  assign dram_cas_n              = 'h1;
  assign dram_we_n               = 'h1;

  assign sram_a                  = 'h0;
  assign sram_dq                 = {16{1'bZ}};
  assign sram_oe_n               = 1;
  assign sram_we_n               = 1;
  assign sram_ub_n               = 1;
  assign sram_lb_n               = 1;

  assign dbg_tx                  = 1'bZ;
  assign user1                   = 1'bZ;
  assign aux_scl                 = 1'bZ;
  assign vpll_feed               = 1'bZ;

  always @(*) begin
    casex (bridge_addr)
      32'hF80020xx: begin
        bridge_rd_data <= dataslot_table_rd_data;
      end
      default: begin
        bridge_rd_data <= cmd_bridge_rd_data;
      end
    endcase
  end

  //
  // host/target command handler
  //
`ifndef __VERILATOR__
  wire reset_n;  // driven by host commands, can be used as core-wide reset
`endif
  wire [31:0] cmd_bridge_rd_data;

  // bridge host commands
  // synchronous to clk_74a
  wire        status_boot_done = pll_core_locked;
  wire        status_setup_done = pll_core_locked;  // rising edge triggers a target command
  wire        status_running = reset_n;  // we are running as soon as reset_n goes high

  wire        osnotify_inmenu;

  // bridge target commands
  // synchronous to clk_74a
  core_bridge_cmd icb (
      .clk                 (clk_74a),
`ifndef __VERILATOR__
      .reset_n             (reset_n),
`endif
      .bridge_endian_little(bridge_endian_little),
      .bridge_addr         (bridge_addr),
      .bridge_rd           (bridge_rd),
      .bridge_rd_data      (cmd_bridge_rd_data),
      .bridge_wr           (bridge_wr),
      .bridge_wr_data      (bridge_wr_data),

      .status_boot_done (status_boot_done),
      .status_setup_done(status_setup_done),
      .status_running   (status_running),

      .osnotify_inmenu(osnotify_inmenu),

      .i_cpu_clk(clk_8mhz),
      .i_cpu_req(cpu_mem_valid && cpu_mem_addr[31:28] == 4'h4),
      .o_cpu_ack_pulse(bridge_ack_pulse),

      .i_cpu_addr (cpu_mem_addr),
      .i_cpu_wdata(cpu_mem_wdata),
      .i_cpu_wstrb(cpu_mem_wstrb),
      .o_cpu_rdata(bridge_rdata)
  );

  //
  // Generate clk_8mhz_1mhz_ph1_en and clk_8mhz_1mhz_ph2_en clock enable pulses
  //

  reg [2:0] clk_8mhz_cntr;
  always @(posedge clk_8mhz) begin
    if (rst) clk_8mhz_cntr <= 0;
    else clk_8mhz_cntr <= clk_8mhz_cntr + 1;
  end
  wire clk_8mhz_1mhz_ph1_en;
  wire clk_8mhz_1mhz_ph2_en;
  assign clk_8mhz_1mhz_ph1_en = (clk_8mhz_cntr == 3'b000);
  assign clk_8mhz_1mhz_ph2_en = (clk_8mhz_cntr == 3'b100);

  //
  // Emulated system clock: the C64 and 1541 run on clk_32mhz and advance only
  // when sys_ce is high. Normally sys_ce fires on the 32 MHz cycle that ends on
  // each 8 MHz edge, so the system behaves exactly as when clocked by clk_8mhz
  // (same edges, same alignment to PSRAM and SID). tk is the system's own
  // 8-step phase counter; it only advances on sys_ce, so a future warp mode
  // can fire sys_ce more often and the phases follow.
  //
  // ref32 restarts from rst together with clk_8mhz_cntr, so
  // ref32[4:2] == clk_8mhz_cntr and ref32[1:0] == 3 marks the last 32 MHz
  // cycle before an 8 MHz edge.
  reg [4:0] ref32;
  always @(posedge clk_32mhz) begin
    if (rst) ref32 <= 0;
    else ref32 <= ref32 + 1'b1;
  end

  reg [2:0] tk;
  wire sys_ce;  // driven by the warp block near the end of the module
  reg warp_active;
  always @(posedge clk_32mhz) begin
    if (rst) tk <= 0;
    else if (sys_ce) tk <= tk + 1'b1;
  end
  wire sys_ph1_en = sys_ce && (tk == 3'b000);
  wire sys_ph2_en = sys_ce && (tk == 3'b100);

  // Synchronize reset used by C64 and C1541 so that first phase seen after
  // reset is ph1
  reg ph_synced_rst;
  always @(posedge clk_32mhz) begin
    if (rst) ph_synced_rst <= 1;
    else if (sys_ph2_en) ph_synced_rst <= ~c64_ctrl[0];
  end

  //
  // audio i2s silence generator
  // see other examples for actual audio generation
  //

  assign audio_mclk = audgen_mclk;
  assign audio_dac  = audgen_dac;
  assign audio_lrck = audgen_lrck;

  // generate MCLK = 12.288mhz with fractional accumulator
  reg [21:0] audgen_accum = 0;
  reg        audgen_mclk;
  parameter [20:0] CYCLE_48KHZ = 21'd122880 * 2;
  always @(posedge clk_74a) begin
    audgen_accum <= audgen_accum + CYCLE_48KHZ;
    if (audgen_accum >= 21'd742500) begin
      audgen_mclk  <= ~audgen_mclk;
      audgen_accum <= audgen_accum - 21'd742500 + CYCLE_48KHZ;
    end
  end

  // generate SCLK = 3.072mhz by dividing MCLK by 4
  reg  [1:0] aud_mclk_divider;
  wire       audgen_sclk = aud_mclk_divider[1]  /* synthesis keep*/;
  reg        audgen_lrck_1;
  always @(posedge audgen_mclk) begin
    aud_mclk_divider <= aud_mclk_divider + 1'b1;
  end

  // shift out audio data as I2S
  // 32 total bits per channel, but only 16 active bits at the start and then 16 dummy bits
  //
  reg [ 4:0] audgen_lrck_cnt;
  reg        audgen_lrck;
  reg        audgen_dac;
  reg [31:0] audgen_shift;
  wire audio_mute;  // warp mode: muted while not in real time
  wire [15:0] sid_audio_l_16 = audio_mute ? 16'h0 : sid_audio_l[17:2];
  wire [15:0] sid_audio_r_16 = audio_mute ? 16'h0 : (sid_dual_s ? sid_audio_r[17:2] : sid_audio_l[17:2]);
  always @(negedge audgen_sclk) begin
    audgen_dac <= audgen_shift[31];
    audgen_shift <= {audgen_shift[30:0], 1'b0};
    // 48khz * 64
    audgen_lrck_cnt <= audgen_lrck_cnt + 1'b1;
    if (audgen_lrck_cnt == 31) begin
      // switch channels: LRCK 0 = Left, LRCK 1 = Right
      audgen_lrck  <= ~audgen_lrck;
      audgen_shift <= audgen_lrck ? {sid_audio_l_16, 16'h0} : {sid_audio_r_16, 16'h0};
    end
  end


  ///////////////////////////////////////////////


  wire clk_8mhz;
  wire clk_8mhz_90deg;
  wire clk_32mhz;
  wire pll_core_locked;

`ifndef __VERILATOR__
  mf_pllbase mp1 (
      .refclk(clk_74a),
      .rst   (0),

      .outclk_0(clk_8mhz),
      .outclk_1(clk_8mhz_90deg),
      .outclk_2(clk_32mhz),

      .locked(pll_core_locked)
  );
`else
  assign clk_8mhz = clk_74a;
  assign clk_8mhz_90deg = clk_74a;
  assign clk_32mhz = clk_74a;
  assign pll_core_locked = 1;
`endif

  // XXX: This is not the right way to do clock domain crossing (don't put
  // syncs on the bus signals)!
  wire [15:0] cont1_key_s;
  wire [15:0] cont2_key_s;
  wire [15:0] cont3_key_s;
  wire [15:0] cont4_key_s;
  wire [31:0] cont1_joy_s;
  wire [31:0] cont2_joy_s;
  wire [31:0] cont3_joy_s;
  wire [31:0] cont4_joy_s;
  wire [15:0] cont1_trig_s;
  wire [15:0] cont2_trig_s;
  wire [15:0] cont3_trig_s;
  wire [15:0] cont4_trig_s;
  synch_3 #(.WIDTH(32)) s_cont1_key (cont1_key, cont1_key_s, clk_8mhz);
  synch_3 #(.WIDTH(32)) s_cont2_key (cont2_key, cont2_key_s, clk_8mhz);
  synch_3 #(.WIDTH(32)) s_cont3_key (cont3_key, cont3_key_s, clk_8mhz);
  synch_3 #(.WIDTH(32)) s_cont4_key (cont4_key, cont4_key_s, clk_8mhz);
  synch_3 #(.WIDTH(32)) s_cont1_joy (cont1_joy, cont1_joy_s, clk_8mhz);
  synch_3 #(.WIDTH(32)) s_cont2_joy (cont2_joy, cont2_joy_s, clk_8mhz);
  synch_3 #(.WIDTH(32)) s_cont3_joy (cont3_joy, cont3_joy_s, clk_8mhz);
  synch_3 #(.WIDTH(32)) s_cont4_joy (cont4_joy, cont4_joy_s, clk_8mhz);
  synch_3 #(.WIDTH(16)) s_cont1_trig (cont1_trig, cont1_trig_s, clk_8mhz);
  synch_3 #(.WIDTH(16)) s_cont2_trig (cont2_trig, cont2_trig_s, clk_8mhz);
  synch_3 #(.WIDTH(16)) s_cont3_trig (cont3_trig, cont3_trig_s, clk_8mhz);
  synch_3 #(.WIDTH(16)) s_cont4_trig (cont4_trig, cont4_trig_s, clk_8mhz);

  wire sid_model_s;
  wire sid_dual_s;
  synch_3 s_sid_model (sid_model, sid_model_s, clk_8mhz);
  synch_3 s_sid_dual  (sid_dual,  sid_dual_s,  clk_8mhz);

  // VIC-II output (clk_32mhz, changes only on sys_ce) registered into the
  // 8 MHz video domain; one uniform stage for sync, enable and colour.
  wire [23:0] c64_vid_rgb;
  wire c64_vid_hs, c64_vid_vs, c64_vid_de;
  // While warp mode runs the system faster than real time (vid_locked = 0),
  // sync comes from the free-running reference raster and the picture is black.
  wire vid_locked;
  wire gen_hs, gen_vs, gen_de;
  reg [23:0] c64_color_rgb;
  reg video_hs_r, video_vs_r, video_de_r;
  always @(posedge clk_8mhz) begin
    c64_color_rgb <= vid_locked ? c64_vid_rgb : 24'h000000;
    video_hs_r <= vid_locked ? c64_vid_hs : gen_hs;
    video_vs_r <= vid_locked ? c64_vid_vs : gen_vs;
    video_de_r <= vid_locked ? c64_vid_de : gen_de;
  end
  assign video_hs = video_hs_r;
  assign video_vs = video_vs_r;
  assign video_de = video_de_r;
  wire [17:0] sid_audio_l;
  wire [17:0] sid_audio_r;
  wire  [7:0] sid_data_out;
  wire        c64_sid_cs;
  wire        c64_sid2_cs;
  wire        c64_sid_we;
  wire  [4:0] c64_sid_addr;
  wire  [7:0] c64_sid_data;

  reg [6:0] joystick1;

  // cont1_key_s bits: 0: Up, 1: Down, 2: Left, 3: Right, 4: Face A, 5: Face B (Jump), 6: Face X, 7: Face Y (Fire), 8: Trig L, 9: Trig R
  // control_mode:
  //   2'b00: Standard (D-pad Up | Face B)
  //   2'b01: Platformer (Face B only, D-pad Up disabled)
  //   2'b10: Driving (Trig R = Up/Gas, Trig L = Down/Brake, D-pad Up/Down disabled)
  wire joy1_up = (control_mode == 2'b10) ? cont1_key_s[9] :
                 (control_mode == 2'b01) ? cont1_key_s[5] :
                 (cont1_key_s[0] | cont1_key_s[5]);

  wire joy1_down = (control_mode == 2'b10) ? cont1_key_s[8] : cont1_key_s[1];

  always @* begin
    case (c64_ctrl[2:1])
      2'b01: begin
        // bit 0: Up, bit 1: Down, bit 2: Left, bit 3: Right, bit 4: Fire 1, bits 5-6: POT
        joystick1 = {2'b00, cont1_key_s[7], cont1_key_s[3:2], joy1_down, joy1_up};
      end
      2'b10: begin
        joystick1 = {2'b00, cont2_key_s[7], cont2_key_s[3:1], cont2_key_s[0] | cont2_key_s[5]};
      end
      default: begin
        joystick1 = 0;
      end
    endcase
  end

  reg [6:0] joystick2;

  always @* begin
    case (c64_ctrl[4:3])
      2'b01: begin
        joystick2 = {2'b00, cont1_key_s[7], cont1_key_s[3:2], joy1_down, joy1_up};
      end
      2'b10: begin
        joystick2 = {2'b00, cont2_key_s[7], cont2_key_s[3:1], cont2_key_s[0] | cont2_key_s[5]};
      end
      default: begin
        joystick2 = 0;
      end
    endcase
  end

  // IEC
  wire iec_atn;
  wire iec_data;
  wire iec_clock;
  wire iec_c64_data_out;
  wire iec_c64_clock_out;
  wire iec_1541_data_out;
  wire iec_1541_clock_out;

  // Any device can pull clock or data low
  assign iec_data = iec_c64_data_out & iec_1541_data_out;
  assign iec_clock = iec_c64_clock_out & iec_1541_clock_out;

  assign debug_iec_atn = iec_atn;
  assign debug_iec_data = iec_data;
  assign debug_iec_clock = iec_clock;
  assign debug_1mhz_ph1_en = sys_ph1_en;
  assign debug_1mhz_ph2_en = sys_ph2_en;

  myc64_top u_myc64 (
      .rst(ph_synced_rst),
      .clk(clk_32mhz),
      .i_ce(sys_ce),
      .i_clk_1mhz_ph1_en(sys_ph1_en),
      .i_clk_1mhz_ph2_en(sys_ph2_en),
      .o_vid_rgb(c64_vid_rgb),
      .o_vid_hsync(c64_vid_hs),
      .o_vid_vsync(c64_vid_vs),
      .o_vid_en(c64_vid_de),
      .i_sid_data(sid_data_out),
      .o_sid_cs(c64_sid_cs),
      .o_sid2_cs(c64_sid2_cs),
      .o_sid_we(c64_sid_we),
      .o_sid_addr(c64_sid_addr),
      .o_sid_data(c64_sid_data),
      .i_keyboard_mask(keyboard_mask),
      .i_joystick1(joystick1),
      .i_joystick2(joystick2),
      .o_bus_addr(c64_bus_addr),
      .i_rom_basic_data(c64_rom_basic_data),
      .i_rom_char_data(c64_rom_char_data),
      .i_rom_kernal_data(c64_rom_kernal_data),
      .i_ram_main_data(c64_ram_rdata),
      .o_ram_main_data(c64_ram_wdata),
      .o_ram_main_we(c64_ram_we),
      .o_iec_atn_out(iec_atn),
      .i_iec_data_in(iec_data),
      .o_iec_data_out(iec_c64_data_out),
      .i_iec_clock_in(iec_clock),
      .o_iec_clock_out(iec_c64_clock_out),
      .i_cart_type(c64_ctrl[6:5]),
      .o_cart_addr(c64_cart_addr),
      .o_cart_we(c64_cart_we),
      .i_cart_data(c64_cart_idata),
      .o_cart_data(c64_cart_odata),
      // Debug signals
      .o_debug_6510_valid(debug_c64_cpu_valid),
      .o_debug_6510_sync(debug_c64_cpu_sync),
      .o_debug_6510_addr(debug_c64_cpu_addr),
      .o_debug_6510_data(debug_c64_cpu_data),
      .o_debug_6510_regs(debug_c64_cpu_regs)
  );

  //
  // reSIDfp SystemVerilog Dual-SID (MOS 6581 / MOS 8580 selectable)
  //
  wire [1:0] sid_chip_cs = { (sid_dual_s & c64_sid2_cs), c64_sid_cs };
  wire [1:0] sid_chip_mode = { sid_model_s, sid_model_s };

  // Generate a 1MHz clock enable in the 32MHz domain.
  // 32 cycles at 32MHz = 1.0MHz period (synchronous with clk_8mhz 1MHz ph2).
  // This gives sid_top all 16 cycles needed to calculate both SID filter/audio channels.
  // Free-running off ref32 so the SID always gets real-time 1 MHz with its
  // full 32-cycle pipeline, one cycle after the system's ph2 step.
  wire sid_ce_1m = (ref32 == 5'd20);

  sid_top #(
      .MULTI_FILTERS(1),
      .DUAL(1)
  ) u_sid (
      .reset      (ph_synced_rst),
      .clk        (clk_32mhz),
      .ce_1m      (sid_ce_1m),
      .cs         (sid_chip_cs),
      .we         (c64_sid_we),
      .addr       (c64_sid_addr),
      .data_in    (c64_sid_data),
      .data_out   (sid_data_out),
      .fc_offset_l(13'd0),
      .pot_x_l    (8'hff),
      .pot_y_l    (8'hff),
      .ext_in_l   (18'sd0),
      .audio_l    (sid_audio_l),
      .fc_offset_r(13'd0),
      .pot_x_r    (8'hff),
      .pot_y_r    (8'hff),
      .ext_in_r   (18'sd0),
      .audio_r    (sid_audio_r),
      .filter_en  (2'b11),
      .mode       (sid_chip_mode),
      .cfg        (4'b0000),
      .ld_clk     (1'b0),
      .ld_addr    (12'd0),
      .ld_data    (16'd0),
      .ld_wr      (1'b0)
  );

  wire [15:0] c1541_bus_addr;
  wire [7:0] c1541_ram_rdata;
  wire [7:0] c1541_ram_wdata;
  wire [7:0] c1541_rom_data;
  wire c1541_ram_we;

  wire [10:0] c1541_track_mem_addr;
  wire [31:0] c1541_track_mem_data;
  wire [6:0] c1541_track_no;
  wire c1541_led_on;
  wire c1541_motor_on;

  my1541_top u_my1541 (
      .rst(ph_synced_rst),
      .clk(clk_32mhz),
      .i_ce(sys_ce),
      .i_clk_1mhz_ph1_en(sys_ph1_en),
      .i_clk_1mhz_ph2_en(sys_ph2_en),
      .o_addr(c1541_bus_addr),
      .i_ram_data(c1541_ram_rdata),
      .o_ram_data(c1541_ram_wdata),
      .o_ram_we(c1541_ram_we),
      .i_rom_data(c1541_rom_data),
      .o_track_addr(c1541_track_mem_addr),
      .i_track_data(c1541_track_mem_data),
      .i_track_len(c1541_track_len),
      .o_track_no(c1541_track_no),
      .o_led_on(c1541_led_on),
      .o_motor_on(c1541_motor_on),
      .i_iec_atn_in(iec_atn),
      .i_iec_data_in(iec_data),
      .o_iec_data_out(iec_1541_data_out),
      .i_iec_clock_in(iec_clock),
      .o_iec_clock_out(iec_1541_clock_out),
      // Debug signals
      .o_debug_6502_valid(debug_c1541_cpu_valid),
      .o_debug_6502_sync(debug_c1541_cpu_sync),
      .o_debug_6502_addr(debug_c1541_cpu_addr),
      .o_debug_6502_data(debug_c1541_cpu_data),
      .o_debug_6502_regs(debug_c1541_cpu_regs)
  );

  wire [15:0] c64_bus_addr;
  wire [7:0] c64_ram_rdata;
  wire [7:0] c64_ram_wdata;
  wire c64_ram_we;

  reg ext_ram_we_r;
  reg ext_ram_done;

  // For better or worse the memory signals need to be stable for an entire ph2
  // cycle. ext_ram_done is a level (cleared when the BIOS drops the request)
  // so the 8 MHz ready logic sees it however the system steps are spaced.
  always @(posedge clk_32mhz) begin
    if (sys_ph2_en) begin
      ext_ram_we_r <= ext_ram_we;
    end
    else if (sys_ph1_en) begin
      ext_ram_we_r <= 0;
    end

    if (!ext_ram_we) ext_ram_done <= 0;
    else if (ext_ram_we_r && sys_ph1_en) ext_ram_done <= 1;
  end


  //
  // Memories for MyC64
  //
  spram #(
      .aw(16),
      .dw(8)
  ) u_c64_main_ram (
      .clk (clk_32mhz),
      .rst (rst),
      .ce  (1'b1),
      .oe  (1'b1),
      .addr(ext_ram_we_r ? ext_addr : c64_bus_addr),
      .do  (c64_ram_rdata),
      .di  (ext_ram_we_r ? ext_data : c64_ram_wdata),
      .we  (ext_ram_we_r | c64_ram_we)
  );

  wire [7:0] c64_rom_char_data;
  spram #(
      .aw(12),
      .dw(8)
  ) u_c64_char_rom (
      .clk (clk_32mhz),
      .rst (rst),
      .ce  (1'b1),
      .oe  (1'b1),
      .addr(ext_rom_char_we ? ext_addr : c64_bus_addr),
      .do  (c64_rom_char_data),
      .di  (ext_data),
      .we  (ext_rom_char_we)
  );

  wire [7:0] c64_rom_basic_data;
  spram #(
      .aw(13),
      .dw(8)
  ) u_c64_basic_rom (
      .clk (clk_32mhz),
      .rst (rst),
      .ce  (1'b1),
      .oe  (1'b1),
      .addr(ext_rom_basic_we ? ext_addr : c64_bus_addr),
      .do  (c64_rom_basic_data),
      .di  (ext_data),
      .we  (ext_rom_basic_we)
  );

  wire [7:0] c64_rom_kernal_data;
  spram #(
      .aw(13),
      .dw(8)
  ) u_c64_kernal_rom (
      .clk (clk_32mhz),
      .rst (rst),
      .ce  (1'b1),
      .oe  (1'b1),
      .addr(ext_rom_kernal_we ? ext_addr : c64_bus_addr),
      .do  (c64_rom_kernal_data),
      .di  (ext_data),
      .we  (ext_rom_kernal_we)
  );

  wire [20:0] c64_cart_addr;
  wire [7:0] c64_cart_idata;
  wire [7:0] c64_cart_odata;
  wire c64_cart_we;
`ifdef __VERILATOR__XXX
  spram #(
      .aw(21),
      .dw(8)
  ) u_c64_cart_rom_lo (
      .clk (clk_8mhz),
      .rst (rst),
      .ce  (1'b1),
      .oe  (1'b1),
      .addr(ext_rom_cart_we ? ext_addr : c64_cart_addr),
      .do  (c64_cart_idata),
      .di  (ext_rom_cart_we ? ext_data : c64_cart_odata),
      .we  (ext_rom_cart_we | c64_cart_we)
  );

`else

  // Generate a clock enable signal in the 32MHz domain that is aligned with
  // the corresponding clk_8mhz_1mhz_ph[12]_en signals in the 8MHz domain.
  reg [3:0] clk_32mhz_cntr;
  wire clk_32mhz_1mhz_ph12_en;
  assign clk_32mhz_1mhz_ph12_en = clk_32mhz_cntr[3:0] == 0;
  always @(posedge clk_32mhz) begin
    if (rst) clk_32mhz_cntr <= 10; // XXX: This is a bit arbitrary but BAD: 13,0,15 GOOD: 10
    else clk_32mhz_cntr <= clk_32mhz_cntr + 1;
  end

  // Then use it to synchronize transactions of the PSRAM controller to the two
  // phases of the C64 core. Since clk_8mhz and clk_32mhz are 'related
  // clocks' the tools should take care of the rest (I hope).
  wire psram_wen;
  assign psram_wen = clk_32mhz_1mhz_ph12_en & (ext_rom_cart_we | c64_cart_we);
  wire psram_ren;
  assign psram_ren = clk_32mhz_1mhz_ph12_en & ~(ext_rom_cart_we | c64_cart_we);

  psram #(
    .CLOCK_SPEED(32)
  ) u_psram (
    .clk(clk_32mhz),
    .bank_sel(1'b0),
    .addr(ext_rom_cart_we ? ext_addr : c64_cart_addr),
    .write_en(psram_wen),
    .data_in(ext_rom_cart_we ? ext_data : c64_cart_odata),
    .write_high_byte(1'b0),
    .write_low_byte(1'b1),

    .read_en(psram_ren),
    .read_avail(),
    .data_out(c64_cart_idata),

    .busy(),

    // PSRAM signals
    .cram_a(cram0_a),
    .cram_dq(cram0_dq),
    .cram_wait(cram0_wait),
    .cram_clk(cram0_clk),
    .cram_adv_n(cram0_adv_n),
    .cram_cre(cram0_cre),
    .cram_ce0_n(cram0_ce0_n),
    .cram_ce1_n(cram0_ce1_n),
    .cram_oe_n(cram0_oe_n),
    .cram_we_n(cram0_we_n),
    .cram_ub_n(cram0_ub_n),
    .cram_lb_n(cram0_lb_n)
  );

`endif

  //
  // Memories for My1541
  //
  spram #(
      .aw(11),
      .dw(8)
  ) u_c1541_ram (
      .clk (clk_32mhz),
      .rst (rst),
      .ce  (1'b1),
      .oe  (1'b1),
      .addr(c1541_bus_addr),
      .do  (c1541_ram_rdata),
      .di  (c1541_ram_wdata),
      .we  (c1541_ram_we)
  );

  spram #(
      .aw(14),
      .dw(8)
  ) u_c1541_rom (
      .clk (clk_32mhz),
      .rst (rst),
      .ce  (1'b1),
      .oe  (1'b1),
      .addr(ext_rom_1541_we ? ext_addr : c1541_bus_addr),
      .do  (c1541_rom_data),
      .di  (ext_data),
      .we  (ext_rom_1541_we)
  );

  wire cpu_mem_valid;
  wire cpu_mem_instr;
  reg cpu_mem_ready;
  wire [31:0] cpu_mem_addr;
  wire [31:0] cpu_mem_wdata;
  wire [3:0] cpu_mem_wstrb;
  reg [31:0] cpu_mem_rdata;
  wire [31:0] ram_rdata, rom_rdata;

  wire [31:0] ram_addr;
  wire [31:0] ram_wdata;
  wire [3:0] ram_wstrb;

  wire [31:0] osd_mem_addr;

  reg [31:0] ext_addr;
  reg [7:0] ext_data; // XXX: Rename to cpu_mem_wdata_byte?
  wire ext_ram_we;
  wire ext_rom_basic_we;
  wire ext_rom_char_we;
  wire ext_rom_kernal_we;
  wire ext_rom_cart_we;
  wire ext_rom_1541_we;

  assign ext_ram_we = (cpu_mem_addr[31:16] == 16'h5000) && cpu_mem_valid && (cpu_mem_wstrb != 0);
  assign ext_rom_basic_we = (cpu_mem_addr[31:16] == 16'h5001) && cpu_mem_valid && (cpu_mem_wstrb != 0);
  assign ext_rom_char_we = (cpu_mem_addr[31:16] == 16'h5002) && cpu_mem_valid && (cpu_mem_wstrb != 0);
  assign ext_rom_kernal_we = (cpu_mem_addr[31:16] == 16'h5003) && cpu_mem_valid && (cpu_mem_wstrb != 0);
  assign ext_rom_1541_we = (cpu_mem_addr[31:16] == 16'h5004) && cpu_mem_valid && (cpu_mem_wstrb != 0);
  assign ext_rom_cart_we = (cpu_mem_addr[31:24] == 8'h51) && cpu_mem_valid && (cpu_mem_wstrb != 0);

  always @* begin
    case (cpu_mem_wstrb)
      4'b0001: begin
        ext_addr = {cpu_mem_addr[31:2], 2'b00};
        ext_data = cpu_mem_wdata[7:0];
      end
      4'b0010: begin
        ext_addr = {cpu_mem_addr[31:2], 2'b01};
        ext_data = cpu_mem_wdata[15:8];
      end
      4'b0100: begin
        ext_addr = {cpu_mem_addr[31:2], 2'b10};
        ext_data = cpu_mem_wdata[23:16];
      end
      4'b1000: begin
        ext_addr = {cpu_mem_addr[31:2], 2'b11};
        ext_data = cpu_mem_wdata[31:24];
      end
      default: begin
        ext_addr = 0;
        ext_data = 0;
      end
    endcase
  end



  wire osd_ram_access;
  wire [31:0] bridge_rdata;
  wire [31:0] bridge_dpram_rdata;

  assign ram_addr  = osd_ram_access ? osd_mem_addr : cpu_mem_addr;
  assign ram_wdata = cpu_mem_wdata;
  assign ram_wstrb = osd_ram_access ? 4'b0000 : cpu_mem_wstrb;

  always @* begin
    casex (cpu_mem_addr)
      32'h0xxx_xxxx: cpu_mem_rdata = rom_rdata;
      32'h1xxx_xxxx: cpu_mem_rdata = ram_rdata;
      32'h2000_0000: cpu_mem_rdata = cont1_key_s;
      32'h2000_0004: cpu_mem_rdata = cont2_key_s;
      32'h2000_0008: cpu_mem_rdata = cont3_key_s;
      32'h2000_000c: cpu_mem_rdata = cont4_key_s;
      32'h2000_0010: cpu_mem_rdata = cont1_joy_s;
      32'h2000_0014: cpu_mem_rdata = cont2_joy_s;
      32'h2000_0018: cpu_mem_rdata = cont3_joy_s;
      32'h2000_001c: cpu_mem_rdata = cont4_joy_s;
      32'h2000_0020: cpu_mem_rdata = cont1_trig_s;
      32'h2000_0024: cpu_mem_rdata = cont2_trig_s;
      32'h2000_0028: cpu_mem_rdata = cont3_trig_s;
      32'h2000_002c: cpu_mem_rdata = cont4_trig_s;
      32'h3000_000c: cpu_mem_rdata = c64_ctrl;
      32'h3000_0100: cpu_mem_rdata = {warp_active, c1541_motor_on, c1541_led_on, c1541_track_no};
      32'h4xxx_xxxx: cpu_mem_rdata = bridge_rdata;
      32'h7xxx_xxxx: cpu_mem_rdata = bridge_dpram_rdata;
      32'h8xxx_xxxx: cpu_mem_rdata = c1541_track_ram_rdata;
      32'h9xxx_xxxx: cpu_mem_rdata = dataslot_table_rd_data_cpu;
      default: cpu_mem_rdata = 0;
    endcase
  end

  reg [1:0] osd_ctrl;
  always @(posedge clk_8mhz) begin
    if (rst) osd_ctrl <= 0;
    else if (cpu_mem_addr == 32'h30000000 && cpu_mem_valid && cpu_mem_wstrb == 4'b1111)
      osd_ctrl <= cpu_mem_wdata;
  end

  reg [6:0] c64_ctrl;
  reg [12:0] c1541_track_len;
  reg [7:0] c1541_track_ack;  // {valid, track_no}: BIOS has this track in track RAM
  always @(posedge clk_8mhz) begin
    if (rst) c64_ctrl <= 0;
    else if (cpu_mem_addr == 32'h3000000c && cpu_mem_valid && cpu_mem_wstrb == 4'b1111)
      c64_ctrl <= cpu_mem_wdata[6:0];
    else if (cpu_mem_addr == 32'h30000108 && cpu_mem_valid && cpu_mem_wstrb == 4'b1111)
      c1541_track_ack <= cpu_mem_wdata[7:0];
    else if (cpu_mem_addr == 32'h30000104 && cpu_mem_valid && cpu_mem_wstrb == 4'b1111) begin
      c1541_track_len <= cpu_mem_wdata[12:0];
      $display("track_len: %d, track_no: %d", c1541_track_len, c1541_track_no);
    end
  end

  reg [63:0] keyboard_mask;
  always @(posedge clk_8mhz) begin
    if (rst) keyboard_mask <= 0;
    else if (cpu_mem_addr == 32'h30000004 && cpu_mem_valid && cpu_mem_wstrb == 4'b1111)
      keyboard_mask[31:0] <= cpu_mem_wdata;
    else if (cpu_mem_addr == 32'h30000008 && cpu_mem_valid && cpu_mem_wstrb == 4'b1111)
      keyboard_mask[63:32] <= cpu_mem_wdata;
  end

  wire bridge_ack_pulse;

  // Synchronize the 'reset_n' signal for safe use in the clk_8mhz domain (as
  // well as the related clk_32mhz domain).
  wire rst;
  synch_3 s_reset_n (~reset_n, rst, clk_8mhz);
  //assign rst = ~reset_n;

  always @(posedge clk_8mhz) begin
    if (rst) cpu_mem_ready <= 0;
    else begin
      casex (cpu_mem_addr)
        32'h0xxx_xxxx: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid;
        32'h1xxx_xxxx: cpu_mem_ready <= ~osd_ram_access & ~cpu_mem_ready & cpu_mem_valid;
        32'h2xxx_xxxx: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid;
        32'h3xxx_xxxx: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid;
        32'h4xxx_xxxx: cpu_mem_ready <= bridge_ack_pulse;
        32'h5000_xxxx: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid & ext_ram_done;
        32'h51xx_xxxx: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid & clk_8mhz_1mhz_ph1_en;
        32'h5xxx_xxxx: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid;
        32'h7xxx_xxxx: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid;
        32'h8xxx_xxxx: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid;
        32'h9xxx_xxxx: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid;
        default: cpu_mem_ready <= 0;
      endcase
    end
  end

  // ROM - CPU code.
  sprom #(
      .aw(12),
      .dw(32),
      .MEM_INIT_FILE("bios.vh")
  ) u_rom (
      .clk (clk_8mhz),
      .rst (rst),
      .ce  (cpu_mem_valid && cpu_mem_addr[31:28] == 4'h0),
      .oe  (1'b1),
      .addr(cpu_mem_addr[13:2]),
      .do  (rom_rdata)
  );

  // RAM - shared between CPU and OSD. OSD has priority.
  genvar gi;
  generate
    for (gi = 0; gi < 4; gi = gi + 1) begin : ram
      spram #(
          .aw(12),
          .dw(8)
      ) u_ram (
          .clk (clk_8mhz),
          .rst (rst),
          .ce  (osd_ram_access || (cpu_mem_valid && cpu_mem_addr[31:28] == 4'h1)),
          .oe  (1'b1),
          .addr(ram_addr[13:2]),
          .do  (ram_rdata[(gi+1)*8-1:gi*8]),
          .di  (ram_wdata[(gi+1)*8-1:gi*8]),
          .we  (ram_wstrb[gi])
      );
    end
  endgenerate

  picorv32 #(
      .COMPRESSED_ISA(1),
      .ENABLE_IRQ(1),
      .ENABLE_MUL(1),
      .ENABLE_DIV(1)
  ) u_cpu (
      .clk(clk_8mhz),
      .resetn(~rst),
      .mem_valid(cpu_mem_valid),
      .mem_instr(cpu_mem_instr),
      .mem_ready(cpu_mem_ready),
      .mem_addr(cpu_mem_addr),
      .mem_wdata(cpu_mem_wdata),
      .mem_wstrb(cpu_mem_wstrb),
      .mem_rdata(cpu_mem_rdata)
  );


  //
  // On Screen Display (OSD)
  //
  reg [7:0] osd_offset_x; // XXX: Unguarded CDC
  reg [7:0] osd_offset_y;
  reg [1:0] control_mode; // 0: Standard, 1: Platformer (No D-Pad Up), 2: Driving (L=Down, R=Up)
  reg       sid_model;    // 0: 6581, 1: 8580
  reg       sid_dual;     // 0: Mono, 1: Dual SID (Stereo)
  reg       warp_mode;    // 0: Off, 1: Auto while the drive motor runs

  initial begin
    osd_offset_x <= 1;
    osd_offset_y <= 1;
    control_mode <= 0;
    sid_model    <= 0; // Default to 6581
    sid_dual     <= 0; // Default to Mono
    warp_mode    <= 0;
  end

  always @(posedge clk_74a) begin
    // Values written by APF during core boot
    if (bridge_wr) begin
      case (bridge_addr)
        32'hA000_0000: osd_offset_x <= bridge_wr_data;
        32'hA000_0004: osd_offset_y <= bridge_wr_data;
        32'hA000_0008: control_mode <= bridge_wr_data[1:0];
        32'hA000_000C: sid_model    <= bridge_wr_data[0];
        32'hA000_0010: sid_dual     <= bridge_wr_data[0];
        32'hA000_0014: warp_mode    <= bridge_wr_data[0];
      endcase
    end
  end

  reg [8:0] video_pos_x;
  reg [8:0] video_pos_y;

  wire osd_match_x = video_pos_x == osd_offset_x;
  wire osd_match_y = video_pos_y == osd_offset_y;

  always @(posedge clk_8mhz) begin
    if (video_vs) begin
      video_pos_y <= 0;
    end else if (video_hs) begin
      video_pos_x <= 0;
      if (video_pos_x != 0)  // I.e. video_de has been active
        video_pos_y <= video_pos_y + 1;
    end else if (video_de) begin
      video_pos_x <= video_pos_x + 1;
    end
  end

  reg [8:0] osd_x;
  reg [8:0] osd_y;

  always @(posedge clk_8mhz) begin
    if (osd_match_x & osd_match_y) begin
      osd_x <= 0;
      osd_y <= 0;
    end else if (osd_match_x) begin
      osd_x <= 0;
      osd_y <= osd_y + 1;
    end else begin
      osd_x <= osd_x + 1;
    end
  end

  // OSD RAM area 256x64 (2KB with 1 bit per pixel)
  wire osd_active;
  assign osd_active = osd_ctrl[0] && video_de && osd_x[8] == 0 && (osd_ctrl[1] ? (osd_y[8:3] == 0) : (osd_y[8:6] == 0));
  assign osd_mem_addr = {osd_y[5:0], osd_x[7:3]};
  assign osd_ram_access = osd_active && osd_x[2:0] == 3'h0;

  reg [7:0] osd_pixshift;
  reg [7:0] osd_mem_rdata;

  reg [9:0] osd_mem_addr_p;
  reg osd_ram_access_p;
  always @(posedge clk_8mhz) begin
    osd_mem_addr_p   <= osd_mem_addr;
    osd_ram_access_p <= osd_ram_access;
  end

  always @(*) begin
    case (osd_mem_addr_p[1:0])
      2'h0: osd_mem_rdata = ram_rdata[7:0];
      2'h1: osd_mem_rdata = ram_rdata[15:8];
      2'h2: osd_mem_rdata = ram_rdata[23:16];
      2'h3: osd_mem_rdata = ram_rdata[31:24];
    endcase
  end

  always @(posedge clk_8mhz) begin
    if (osd_ram_access_p) osd_pixshift <= osd_mem_rdata;
    else osd_pixshift <= {osd_pixshift[6:0], 1'b0};
  end

  wire [23:0] osd_rgb;
  assign osd_rgb = osd_pixshift[7] ? 24'hff_ff_ff : 24'h30_40_50;

  assign video_rgb_clock = clk_8mhz;
  assign video_rgb_clock_90 = clk_8mhz_90deg;
  assign video_rgb = video_de ? (osd_active ? osd_rgb : c64_color_rgb) : 0;
  assign video_skip = 0;

  bram_block_dp #(
      .DATA(32),
      .ADDR(8) // 1024 bytes in total
  ) u_bridge_dpram (
      .a_clk(clk_74a),
      .a_wr(bridge_wr && bridge_addr[31:28] == 4'h7),
      .a_addr(bridge_addr[31:2]),
      .a_din({
        bridge_wr_data[7:0], bridge_wr_data[15:8], bridge_wr_data[23:16], bridge_wr_data[31:24]
      }),
      .a_dout(  /* NC */),

      .b_clk (clk_8mhz),
      .b_wr  (1'b0),
      .b_addr(cpu_mem_addr[31:2]),
      .b_din (32'h0),
      .b_dout(bridge_dpram_rdata)
  );

  wire [31:0] dataslot_table_rd_data;
  wire [31:0] dataslot_table_rd_data_cpu;
  bram_block_dp #(
      .DATA(32),
      .ADDR(6)
  ) u_bridge_dataslot_table (
      .a_clk (clk_74a),
      .a_wr  (bridge_wr && bridge_addr[31:8] == 24'hF80020),
      .a_addr(bridge_addr[31:2]),
      .a_din (bridge_wr_data),
      .a_dout(dataslot_table_rd_data),

      .b_clk (clk_8mhz),
      .b_wr  (1'b0),
      .b_addr(cpu_mem_addr[31:2]),
      .b_din (32'h0),
      .b_dout(dataslot_table_rd_data_cpu)
  );

  // 8KB of DP track memory for 1541. Fed by bridge or CPU, read by 1541
  wire [31:0] c1541_track_ram_rdata;
  wire bridge_track_ram_wr = bridge_wr && bridge_addr[31:28] == 4'h9;

  // CPU writes (D64 track synthesis) are latched in clk_8mhz and replayed in
  // clk_74a, so port A stays on a single clock. A CPU access lasts >= 2 clk_8mhz
  // cycles, which leaves the latched values stable well past the synchronizer.
  reg [10:0] cpu_track_wr_addr;
  reg [31:0] cpu_track_wr_data;
  reg [3:0] cpu_track_wr_strb;
  reg cpu_track_wr_toggle = 0;
  always @(posedge clk_8mhz) begin
    if (cpu_mem_valid && !cpu_mem_ready && cpu_mem_addr[31:28] == 4'h8 && |cpu_mem_wstrb) begin
      cpu_track_wr_addr   <= cpu_mem_addr[12:2];
      cpu_track_wr_data   <= cpu_mem_wdata;
      cpu_track_wr_strb   <= cpu_mem_wstrb;
      cpu_track_wr_toggle <= ~cpu_track_wr_toggle;
    end
  end

  reg [2:0] cpu_track_wr_sync = 0;
  always @(posedge clk_74a) cpu_track_wr_sync <= {cpu_track_wr_sync[1:0], cpu_track_wr_toggle};
  wire cpu_track_wr_74 = cpu_track_wr_sync[2] ^ cpu_track_wr_sync[1];

  // Bridge data is big-endian; byte-swap so file byte n lands in lane n%4
  wire [31:0] bridge_track_wr_data = {
    bridge_wr_data[7:0], bridge_wr_data[15:8], bridge_wr_data[23:16], bridge_wr_data[31:24]
  };

  genvar ti;
  generate
    for (ti = 0; ti < 4; ti = ti + 1) begin : track_ram_lane
      bram_block_dp #(
          .DATA(8),
          .ADDR(11)
      ) u_track_lane (
          .a_clk (clk_74a),
          .a_wr  (cpu_track_wr_74 ? cpu_track_wr_strb[ti] : bridge_track_ram_wr),
          .a_addr(cpu_track_wr_74 ? cpu_track_wr_addr : bridge_addr[12:2]),
          .a_din (cpu_track_wr_74 ? cpu_track_wr_data[(ti+1)*8-1:ti*8] :
                                    bridge_track_wr_data[(ti+1)*8-1:ti*8]),
          .a_dout(c1541_track_ram_rdata[(ti+1)*8-1:ti*8]),

          .b_clk (clk_32mhz),
          .b_wr  (1'b0),
          .b_addr(c1541_track_mem_addr),
          .b_din (8'h0),
          .b_dout(c1541_track_mem_data[(ti+1)*8-1:ti*8])
      );
    end
  endgenerate

  //
  // Warp mode
  //
  // While the 1541 motor runs, sys_ce fires on every 32 MHz cycle so the C64
  // and the drive run 4x faster, still in lockstep with each other, so every
  // loader works. Video sync then comes from a reference copy of the VIC-II
  // raster that keeps real time; the picture is black and audio is muted.
  // When warp ends the system stalls until its raster lines up with the
  // reference again (at most one frame) and the VIC-II takes over seamlessly.
  //
  wire warp_mode_s;
  synch_3 s_warp_mode (warp_mode, warp_mode_s, clk_32mhz);

  wire ref_tick = (ref32[1:0] == 2'b11);  // real-time 8 MHz step
  wire ce_1x = ref_tick && (tk == ref32[4:2]);

  // Shadow of the VIC-II raster counters (vicii.py: x wraps at 0x1f7, y at
  // 311, cycle restarts at x == 0x190 and counts ph1). Like the VIC-II they
  // reset with ph_synced_rst regardless of sys_ce.
  localparam [8:0] VIC_X_LAST = 9'h190;
  reg [8:0] sx, sy;
  reg [5:0] scyc;
  wire [8:0] sx_n = (sx == 9'h1f7) ? 9'd0 : sx + 1'b1;
  wire [8:0] sy_n = (sx == VIC_X_LAST) ? ((sy == 9'd311) ? 9'd0 : sy + 1'b1) : sy;
  wire [5:0] scyc_n = (sx == VIC_X_LAST) ? 6'd1 : (tk == 3'b000) ? scyc + 1'b1 : scyc;
  wire [8:0] sx_d = ph_synced_rst ? 9'd0 : sys_ce ? sx_n : sx;
  wire [8:0] sy_d = ph_synced_rst ? 9'd0 : sys_ce ? sy_n : sy;
  wire [5:0] scyc_d = ph_synced_rst ? 6'd0 : sys_ce ? scyc_n : scyc;
  always @(posedge clk_32mhz) begin
    sx <= sx_d;
    sy <= sy_d;
    scyc <= scyc_d;
  end

  // Reference raster: follows the shadow while locked, free-runs in real time
  // otherwise.
  reg vid_locked_r;
  reg [8:0] rx, ry;
  reg [5:0] rcyc;
  wire [8:0] rx_n = (rx == 9'h1f7) ? 9'd0 : rx + 1'b1;
  wire [8:0] ry_n = (rx == VIC_X_LAST) ? ((ry == 9'd311) ? 9'd0 : ry + 1'b1) : ry;
  wire [5:0] rcyc_n = (rx == VIC_X_LAST) ? 6'd1 : (ref32[4:2] == 3'b000) ? rcyc + 1'b1 : rcyc;
  always @(posedge clk_32mhz) begin
    if (vid_locked_r) begin
      rx <= sx_d;
      ry <= sy_d;
      rcyc <= scyc_d;
    end else if (ref_tick) begin
      rx <= rx_n;
      ry <= ry_n;
      rcyc <= rcyc_n;
    end
  end

  assign gen_hs = (rx == VIC_X_LAST);
  assign gen_vs = (ry == 9'd0) && (rx == VIC_X_LAST - 9'd3);
  assign gen_de = (rcyc >= 6'd13) && (rcyc < 6'd61) && (ry >= 9'h010) && (ry <= 9'h117);

  // Auto trigger: motor on for ~20 ms starts warp, off for ~250 ms ends it
  // (real time, counted in ref_tick = 8 MHz).
  localparam [21:0] MOTOR_ON_TICKS = 22'd160000;
  localparam [21:0] MOTOR_OFF_TICKS = 22'd2000000;
  reg warp_req;
  reg [21:0] motor_tmr;
  always @(posedge clk_32mhz) begin
    if (rst) begin
      warp_req <= 0;
      motor_tmr <= 0;
    end else if (ref_tick) begin
      if (c1541_motor_on == warp_req) motor_tmr <= 0;
      else if (motor_tmr >= (c1541_motor_on ? MOTOR_ON_TICKS : MOTOR_OFF_TICKS)) begin
        warp_req <= c1541_motor_on;
        motor_tmr <= 0;
      end else motor_tmr <= motor_tmr + 1'b1;
    end
  end

  // A new controller or keyboard press during warp cancels it until the motor
  // stops (the player wants to interact; a held key would also last 4x
  // longer in emulated time). Releases don't count: D64 autoboot lets go of
  // RETURN after the load has already started.
  reg [15:0] key_prev;
  reg [63:0] kbd_prev;
  reg warp_inhibit;
  always @(posedge clk_32mhz) begin
    key_prev <= cont1_key_s;
    kbd_prev <= keyboard_mask;
    if (rst || !warp_req) warp_inhibit <= 0;
    else if (warp_active && ((|(cont1_key_s & ~key_prev)) || (|(keyboard_mask & ~kbd_prev)))) warp_inhibit <= 1;
  end

  // No warp with a cartridge (PSRAM accesses are paced in real time) or
  // during C64 reset.
  always @(posedge clk_32mhz) begin
    if (rst) warp_active <= 0;
    else warp_active <= warp_mode_s && warp_req && !warp_inhibit && (c64_ctrl[6:5] == 2'b00) && !ph_synced_rst;
  end

  // Track change freeze: in warp the BIOS's 10 ms track synthesis delay would
  // be 40 ms of emulated time, so once the head has settled (~1 ms emulated)
  // the system waits until the BIOS acknowledges the new track. Never while
  // the BIOS is blocked on a C64 RAM write, and never longer than ~100 ms.
  localparam [13:0] TRACK_SETTLE_STEPS = 14'd8000;
  localparam [19:0] FREEZE_TIMEOUT_TICKS = 20'd800000;
  reg [6:0] trk_prev;
  reg [13:0] trk_settle;
  reg [19:0] frz_tmr;
  reg frz_timeout;
  wire trk_pending = c1541_track_ack[7] && (c1541_track_ack[6:0] != c1541_track_no);
  wire warp_freeze = warp_active && trk_pending && (trk_settle >= TRACK_SETTLE_STEPS) &&
                     !ext_ram_we && !frz_timeout;
  always @(posedge clk_32mhz) begin
    trk_prev <= c1541_track_no;
    if (c1541_track_no != trk_prev) trk_settle <= 0;
    else if (sys_ce && trk_settle != 14'h3fff) trk_settle <= trk_settle + 1'b1;

    if (!trk_pending) frz_timeout <= 0;
    else if (frz_tmr == FREEZE_TIMEOUT_TICKS) frz_timeout <= 1;

    if (!warp_freeze) frz_tmr <= 0;
    else if (ref_tick) frz_tmr <= frz_tmr + 1'b1;
  end

  // Relock: after warp, step only when the shadow raster equals the reference
  // (same x, y, cycle and phase). If that ever fails, give up after ~50 ms and
  // accept a one-off sync jump rather than stalling.
  localparam [18:0] RELOCK_TIMEOUT_TICKS = 19'd400000;
  wire relock_match = ce_1x && (sx == rx) && (sy == ry) && (scyc == rcyc);
  reg [18:0] relock_tmr;
  always @(posedge clk_32mhz) begin
    if (rst) begin
      vid_locked_r <= 1;
      relock_tmr <= 0;
    end else if (warp_active) begin
      vid_locked_r <= 0;
      relock_tmr <= 0;
    end else if (!vid_locked_r) begin
      if (relock_match || relock_tmr == RELOCK_TIMEOUT_TICKS) vid_locked_r <= 1;
      if (ref_tick) relock_tmr <= relock_tmr + 1'b1;
    end
  end
  wire relock_now = !vid_locked_r && (relock_match || (relock_tmr == RELOCK_TIMEOUT_TICKS && ce_1x));

  assign sys_ce = warp_active ? !warp_freeze : (vid_locked_r ? ce_1x : relock_now);
  assign vid_locked = vid_locked_r;
  assign audio_mute = !vid_locked_r;

endmodule

