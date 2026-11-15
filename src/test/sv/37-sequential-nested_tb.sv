// nested(3) = 3*3 = 9. Module name matches the function in accept/37-sequential-nested.
module nested_tb;
  logic        clk = 0, rst = 1, start = 0, done;
  logic [31:0] n, out;
  int          ciclos = 0;

  nested dut (.clk(clk), .rst(rst), .start(start), .n(n), .done(done), .out(out));

  always #5 clk = ~clk;

  initial begin
    n = 3;
    @(negedge clk); rst = 0;
    @(negedge clk); start = 1;
    @(negedge clk); start = 0;
    while (!done) begin
      @(negedge clk);
      ciclos++;
      if (ciclos > 500) $fatal(1, "timeout");
    end
    if (out !== 9) $fatal(1, "nested = %0d, expected 9", out);
    $display("nested(3) = %0d (%0d ciclos)", out, ciclos);
    $display("PASS");
    $finish;
  end
endmodule
