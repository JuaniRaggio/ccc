module clamp_tb;
  logic signed [31:0] x, tope, out;

  clamp dut (.x(x), .tope(tope), .out(out));

  task automatic check(input logic signed [31:0] a, input logic signed [31:0] t, input logic signed [31:0] expected);
    x = a; tope = t;
    #1;
    if (out !== expected) $fatal(1, "clamp(%0d, %0d) = %0d, expected %0d", a, t, out, expected);
    $display("clamp(%0d, %0d) = %0d", a, t, out);
  endtask

  initial begin
    check(5, 10, 5);
    check(15, 10, 10);
    check(-3, 10, -3);
    $display("PASS");
    $finish;
  end
endmodule
