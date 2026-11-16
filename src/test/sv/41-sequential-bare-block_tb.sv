// bloque(5) = 5 + 3 = 8. The bare blocks (in the preamble and in the loop body) must be
// emitted, not dropped. Module name matches accept/41-sequential-bare-block.
module bloque_tb;
  logic        clk = 0, rst = 1, start = 0, done;
  logic [31:0] n, out;
  int          ciclos = 0;

  bloque dut (.clk(clk), .rst(rst), .start(start), .n(n), .done(done), .out(out));

  always #5 clk = ~clk;

  initial begin
    n = 5;
    @(negedge clk); rst = 0;
    @(negedge clk); start = 1;
    @(negedge clk); start = 0;
    while (!done) begin
      @(negedge clk);
      ciclos++;
      if (ciclos > 500) $fatal(1, "timeout");
    end
    if (out !== 8) $fatal(1, "bloque = %0d, expected 8", out);
    $display("bloque(5) = %0d (%0d ciclos)", out, ciclos);
    $display("PASS");
    $finish;
  end
endmodule
