// twoloops(4) = (0+1+2+3)*2 = 12. Module name matches accept/38-sequential-two-loops.
module twoloops_tb;
  logic        clk = 0, rst = 1, start = 0, done;
  logic [31:0] n, out;
  int          ciclos = 0;

  twoloops dut (.clk(clk), .rst(rst), .start(start), .n(n), .done(done), .out(out));

  always #5 clk = ~clk;

  initial begin
    n = 4;
    @(negedge clk); rst = 0;
    @(negedge clk); start = 1;
    @(negedge clk); start = 0;
    while (!done) begin
      @(negedge clk);
      ciclos++;
      if (ciclos > 500) $fatal(1, "timeout");
    end
    if (out !== 12) $fatal(1, "twoloops = %0d, expected 12", out);
    $display("twoloops(4) = %0d (%0d ciclos)", out, ciclos);
    $display("PASS");
    $finish;
  end
endmodule
