// Drives the start/done handshake of the generated FSM and checks the result.
// sum_to(10) = 0 + 1 + ... + 9 = 45.
module sum_to_tb;
  logic        clk = 0, rst = 1, start = 0, done;
  logic [31:0] n, out;
  int          ciclos = 0;

  sum_to dut (.clk(clk), .rst(rst), .start(start), .n(n), .done(done), .out(out));

  always #5 clk = ~clk;

  initial begin
    n = 10;
    // Inputs are driven and outputs read on the falling edge, so they do not
    // race the rising edge on which the FSM advances.
    @(negedge clk); rst = 0;
    @(negedge clk); start = 1;
    @(negedge clk); start = 0;
    while (!done) begin
      @(negedge clk);
      ciclos++;
      if (ciclos > 100) $fatal(1, "timeout");
    end
    if (out !== 45) $fatal(1, "sum_to = %0d, expected 45", out);
    $display("sum_to(10) = %0d (%0d ciclos)", out, ciclos);
    $display("PASS");
    $finish;
  end
endmodule
