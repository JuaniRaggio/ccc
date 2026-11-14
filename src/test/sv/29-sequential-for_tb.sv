// Drives the start/done handshake of the generated FSM and checks the result.
// factorial(5) = 1 * 2 * 3 * 4 * 5 = 120.
module factorial_tb;
  logic        clk = 0, rst = 1, start = 0, done;
  logic [31:0] n, out;
  int          ciclos = 0;

  factorial dut (.clk(clk), .rst(rst), .start(start), .n(n), .done(done), .out(out));

  always #5 clk = ~clk;

  initial begin
    n = 5;
    @(negedge clk); rst = 0;
    @(negedge clk); start = 1;
    @(negedge clk); start = 0;
    while (!done) begin
      @(negedge clk);
      ciclos++;
      if (ciclos > 100) $fatal(1, "timeout");
    end
    if (out !== 120) $fatal(1, "factorial = %0d, expected 120", out);
    $display("factorial(5) = %0d (%0d ciclos)", out, ciclos);
    $display("PASS");
    $finish;
  end
endmodule
