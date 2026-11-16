// acumular(r = 5, n = 4): r = 5 + 0 + 1 + 2 + 3 = 11. The read-write reference is split
// into r_in (loaded on start) and the output register r. Module name matches accept/39.
module acumular_tb;
  logic        clk = 0, rst = 1, start = 0, done;
  logic [31:0] r_in, r, n;
  int          ciclos = 0;

  acumular dut (.clk(clk), .rst(rst), .start(start), .done(done), .r_in(r_in), .r(r), .n(n));

  always #5 clk = ~clk;

  initial begin
    r_in = 5;
    n = 4;
    @(negedge clk); rst = 0;
    @(negedge clk); start = 1;
    @(negedge clk); start = 0;
    while (!done) begin
      @(negedge clk);
      ciclos++;
      if (ciclos > 500) $fatal(1, "timeout");
    end
    if (r !== 11) $fatal(1, "acumular = %0d, expected 11", r);
    $display("acumular(5, 4) = %0d (%0d ciclos)", r, ciclos);
    $display("PASS");
    $finish;
  end
endmodule
